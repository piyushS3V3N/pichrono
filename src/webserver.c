#include "mongoose.h"
#include "pichrono.h"
#include "filehandler.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static const char *s_http_addr = "http://localhost:%d";

typedef struct {
    char name[128];
    char sha[41];
} BranchRef;

typedef struct {
    char sha[41];
    char message[256];
    char parent[41];
    char branches[10][64];
    int branch_count;
} CommitNode;

static void json_escape(char *dest, const char *src, size_t max_len) {
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d < max_len - 2; s++) {
        if (src[s] == '"' || src[s] == '\\' || src[s] == '\n' || src[s] == '\r') {
            dest[d++] = ' ';
        } else {
            dest[d++] = src[s];
        }
    }
    dest[d] = '\0';
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    
    if (mg_match(hm->uri, mg_str("/api/graph"), NULL)) {
        BranchRef refs[64];
        int ref_count = 0;
        DIR *d = opendir(".pichrono/refs/heads");
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL && ref_count < 64) {
                if (dir->d_name[0] == '.') continue;
                char path[512];
                snprintf(path, sizeof(path), ".pichrono/refs/heads/%s", dir->d_name);
                size_t s;
                char *content = filehandler_read_all(path, &s);
                if (content) {
                    strncpy(refs[ref_count].name, dir->d_name, 127);
                    sscanf(content, "%40s", refs[ref_count].sha);
                    ref_count++;
                    free(content);
                }
            }
            closedir(d);
        }

        CommitNode *nodes = malloc(sizeof(CommitNode) * 100);
        int node_count = 0;
        char queue[100][41];
        int q_head = 0, q_tail = 0;

        for (int i = 0; i < ref_count; i++) {
            strcpy(queue[q_tail++], refs[i].sha);
        }

        while (q_head < q_tail && node_count < 100) {
            char current_sha[41];
            strcpy(current_sha, queue[q_head++]);

            int found = 0;
            for (int i = 0; i < node_count; i++) {
                if (strcmp(nodes[i].sha, current_sha) == 0) {
                    found = 1; break;
                }
            }
            if (found) continue;

            size_t os;
            char *obj = read_object(current_sha, &os);
            if (!obj) continue;

            strcpy(nodes[node_count].sha, current_sha);
            nodes[node_count].branch_count = 0;
            for (int i = 0; i < ref_count; i++) {
                if (strcmp(refs[i].sha, current_sha) == 0) {
                    strncpy(nodes[node_count].branches[nodes[node_count].branch_count++], refs[i].name, 63);
                }
            }

            char *content = obj + strlen(obj) + 1;
            nodes[node_count].parent[0] = '\0';
            char *p_ptr = strstr(content, "parent ");
            if (p_ptr) {
                sscanf(p_ptr, "parent %40s", nodes[node_count].parent);
                if (q_tail < 100) strcpy(queue[q_tail++], nodes[node_count].parent);
            }

            char raw_msg[256] = {0};
            char *m_ptr = strstr(content, "\n\n");
            if (m_ptr) {
                m_ptr += 2;
                char *newline = strchr(m_ptr, '\n');
                int len = newline ? (int)(newline - m_ptr) : (int)strlen(m_ptr);
                if (len > 255) len = 255;
                memcpy(raw_msg, m_ptr, len);
                raw_msg[len] = '\0';
                json_escape(nodes[node_count].message, raw_msg, 255);
            }
            free(obj);
            node_count++;
        }

        char *json = malloc(32768);
        strcpy(json, "[");
        for (int i = 0; i < node_count; i++) {
            if (i > 0) strcat(json, ",");
            strcat(json, "{");
            sprintf(json + strlen(json), "\"sha\": \"%s\", \"message\": \"%s\", \"parent\": \"%s\", \"refs\": [", nodes[i].sha, nodes[i].message, nodes[i].parent);
            for (int j = 0; j < nodes[i].branch_count; j++) {
                if (j > 0) strcat(json, ",");
                sprintf(json + strlen(json), "\"%s\"", nodes[i].branches[j]);
            }
            strcat(json, "]}");
        }
        strcat(json, "]");
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
        free(json);
        free(nodes);
    } else if (mg_match(hm->uri, mg_str("/api/branches"), NULL)) {
        char *json = malloc(4096);
        strcpy(json, "{\"branches\": [");
        DIR *d = opendir(".pichrono/refs/heads");
        if (d) {
            struct dirent *dir;
            int first = 1;
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_name[0] == '.') continue;
                if (!first) strcat(json, ",");
                strcat(json, "\"");
                strcat(json, dir->d_name);
                strcat(json, "\"");
                first = 0;
            }
            closedir(d);
        }
        strcat(json, "]}");
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
        free(json);
    } else if (mg_match(hm->uri, mg_str("/api/checkout"), NULL)) {
        char target[256] = {0};
        mg_http_get_var(&hm->body, "target", target, sizeof(target));
        if (strlen(target) > 0) {
            pc_checkout(target);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\": \"ok\"}");
        } else {
            mg_http_reply(c, 400, "", "Missing target");
        }
    } else if (mg_match(hm->uri, mg_str("/api/commit"), NULL)) {
        char msg[256] = {0};
        mg_http_get_var(&hm->body, "message", msg, sizeof(msg));
        if (strlen(msg) > 0) {
            pc_commit(msg);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\": \"ok\"}");
        } else {
            mg_http_reply(c, 400, "", "Missing message");
        }
    } else if (mg_match(hm->uri, mg_str("/api/branch/create"), NULL)) {
        char name[256] = {0};
        mg_http_get_var(&hm->body, "name", name, sizeof(name));
        if (strlen(name) > 0) {
            pc_branch(name);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\": \"ok\"}");
        } else {
            mg_http_reply(c, 400, "", "Missing name");
        }
    } else if (mg_match(hm->uri, mg_str("/"), NULL)) {
        const char *html = 
            "<!DOCTYPE html><html><head>"
            "<script src='https://cdn.tailwindcss.com'></script>"
            "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css'>"
            "<title>Pichrono Visual Dashboard</title>"
            "<style>"
            "  .node { stroke: #1e293b; stroke-width: 2px; filter: drop-shadow(0 0 8px rgba(59, 130, 246, 0.5)); }"
            "  .link { fill: none; stroke: #475569; stroke-width: 2px; stroke-linecap: round; }"
            "  #graph-container::-webkit-scrollbar { width: 6px; height: 6px; }"
            "  #graph-container::-webkit-scrollbar-thumb { background: #334155; border-radius: 10px; }"
            "</style>"
            "</head><body class='bg-slate-900 text-slate-200 min-h-screen font-sans'>"
            "<nav class='border-b border-slate-800 bg-slate-900/80 backdrop-blur-md sticky top-0 z-10'>"
            "  <div class='max-w-7xl mx-auto px-6 py-4 flex justify-between items-center'>"
            "    <div class='flex items-center gap-3'><div class='w-10 h-10 bg-gradient-to-br from-blue-600 to-blue-400 rounded-xl flex items-center justify-center shadow-lg shadow-blue-500/20'><i class='fas fa-project-diagram text-white'></i></div>"
            "    <span class='text-2xl font-bold tracking-tight text-white'>Pichrono</span></div>"
            "    <div class='flex items-center gap-4'>"
            "      <div class='flex flex-col items-end mr-2'><span class='text-[10px] text-slate-500 uppercase font-bold'>Active Branch</span>"
            "      <select id='branch-selector' onchange='window.checkout(this.value)' class='bg-slate-800 text-blue-400 border border-slate-700 rounded-lg px-4 py-1 text-xs font-mono outline-none focus:ring-2 ring-blue-500/50 appearance-none cursor-pointer'></select></div>"
            "      <button onclick='window.loadAll()' class='w-10 h-10 flex items-center justify-center bg-slate-800 hover:bg-slate-700 rounded-full transition-all border border-slate-700 text-slate-400 hover:text-white'><i class='fas fa-sync-alt'></i></button>"
            "    </div>"
            "  </div></nav>"
            "<main class='max-w-7xl mx-auto p-8'><div class='grid grid-cols-1 lg:grid-cols-4 gap-8'>"
            "  <div class='lg:col-span-3'>"
            "    <section class='bg-slate-800/40 rounded-3xl border border-slate-700/50 overflow-hidden shadow-2xl backdrop-blur-sm'>"
            "      <div class='px-8 py-6 border-b border-slate-700/50 bg-slate-800/50 flex justify-between items-center'>"
            "        <h2 class='text-lg font-bold flex items-center gap-3 text-white'><i class='fas fa-microchip text-blue-500'></i> Repository Visual Graph</h2>"
            "        <div class='flex gap-2' id='stats'></div>"
            "      </div>"
            "      <div id='graph-container' class='p-0 overflow-auto bg-slate-900/30 relative' style='height: 700px;'>"
            "        <svg id='graph-svg' width='100%' height='2000' class='min-w-full'></svg>"
            "      </div>"
            "    </section></div>"
            "  <div class='space-y-6'>"
            "    <section class='bg-slate-800/50 rounded-2xl border border-slate-700 p-6'><h3 class='font-bold mb-6 text-white flex items-center gap-2'><i class='fas fa-terminal text-yellow-500'></i> Control Center</h3>"
            "      <div class='grid grid-cols-1 gap-4'>"
            "        <button onclick='window.newCommit()' class='w-full text-left px-5 py-4 bg-blue-600 hover:bg-blue-500 rounded-2xl font-bold transition-all transform hover:-translate-y-1 shadow-lg shadow-blue-500/20 text-white flex items-center justify-between'><span>Take Snapshot</span><i class='fas fa-camera-retro'></i></button>"
            "        <button onclick='window.newBranch()' class='w-full text-left px-5 py-4 bg-slate-700 hover:bg-slate-600 rounded-2xl font-bold transition-all transform hover:-translate-y-1 text-slate-200 flex items-center justify-between border border-slate-600'><span>New Branch</span><i class='fas fa-code-fork'></i></button>"
            "      </div></section>"
            "    <section class='bg-slate-800/50 rounded-2xl border border-slate-700 p-6'><h3 class='font-bold mb-4 text-white'>Visual Guide</h3>"
            "      <div class='space-y-4 text-xs text-slate-400'>"
            "        <div class='flex items-center gap-3 p-2 bg-slate-900/50 rounded-lg'><div class='w-3 h-3 rounded-full bg-blue-500 shadow-[0_0_8px_rgba(59,130,246,0.8)]'></div><span>Stable Commit</span></div>"
            "        <div class='flex items-center gap-3 p-2 bg-slate-900/50 rounded-lg'><div class='w-6 h-0.5 bg-slate-600 rounded-full'></div><span>Lineage Link</span></div>"
            "        <div class='flex items-center gap-3 p-2 bg-slate-900/50 rounded-lg'><span class='px-2 py-0.5 bg-green-500/20 text-green-400 rounded-md border border-green-500/30 font-bold font-mono'>HEAD</span><span>Pointer Location</span></div>"
            "      </div></section>"
            "  </div></div></main>"
            "<script>"
            "var colors = ['#3b82f6', '#10b981', '#f59e0b', '#ef4444', '#8b5cf6', '#ec4899'];"
            "window.loadBranches = function() {"
            "  fetch('/api/branches').then(function(r){return r.json();}).then(function(data) {"
            "    var sel = document.getElementById('branch-selector');"
            "    sel.innerHTML = data.branches.map(function(b){return '<option value=\"'+b+'\">'+b+'</option>';}).join('');"
            "  });"
            "};"
            "window.loadGraph = function() {"
            "  fetch('/api/graph').then(function(r){return r.json();}).then(function(data) {"
            "    var svg = document.getElementById('graph-svg');"
            "    svg.innerHTML = '';"
            "    if (!data || data.length === 0) return;"
            "    document.getElementById('stats').innerHTML = '<span class=\"text-[10px] text-slate-500 uppercase font-bold tracking-widest px-3 py-1 bg-slate-900 rounded-full border border-slate-700\">' + data.length + ' Commits Visualized</span>';"
            "    var commitMap = {}; data.forEach(function(c, i){ commitMap[c.sha] = {c:c, i:i, lane:0}; });"
            "    var lanes = [];"
            "    data.forEach(function(c) {"
            "      var node = commitMap[c.sha]; var assigned = false;"
            "      for(var i=0; i<lanes.length; i++) { if(lanes[i] === c.sha) { node.lane = i; assigned = true; break; } }"
            "      if(!assigned) { node.lane = lanes.length; lanes.push(c.sha); }"
            "      if(c.parent && commitMap[c.parent]) { lanes[node.lane] = c.parent; } else { lanes[node.lane] = null; }"
            "    });"
            "    var y_step = 80; var x_step = 60; var padding_x = 80; var padding_y = 60;"
            "    data.forEach(function(c, i) {"
            "      var node = commitMap[c.sha]; var x = padding_x + node.lane * x_step; var y = padding_y + i * y_step;"
            "      if(c.parent && commitMap[c.parent]) {"
            "        var pnode = commitMap[c.parent]; var px = padding_x + pnode.lane * x_step; var py = padding_y + pnode.i * y_step;"
            "        var path = document.createElementNS('http://www.w3.org/2000/svg', 'path');"
            "        var d = '';"
            "        if (x === px) { d = 'M ' + x + ' ' + y + ' L ' + px + ' ' + py; }"
            "        else { d = 'M ' + x + ' ' + y + ' C ' + x + ' ' + (y + 40) + ' ' + px + ' ' + (py - 40) + ' ' + px + ' ' + py; }"
            "        path.setAttribute('d', d); path.setAttribute('class', 'link'); svg.appendChild(path);"
            "      }"
            "    });"
            "    data.forEach(function(c, i) {"
            "      var node = commitMap[c.sha]; var x = padding_x + node.lane * x_step; var y = padding_y + i * y_step;"
            "      var g = document.createElementNS('http://www.w3.org/2000/svg', 'g');"
            "      var circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');"
            "      circle.setAttribute('cx', x); circle.setAttribute('cy', y); circle.setAttribute('r', 7); circle.setAttribute('class', 'node');"
            "      circle.setAttribute('fill', colors[node.lane % colors.length]); g.appendChild(circle);"
            "      var txt = document.createElementNS('http://www.w3.org/2000/svg', 'text');"
            "      txt.setAttribute('x', x + 25); txt.setAttribute('y', y + 4); txt.setAttribute('fill', '#f8fafc');"
            "      txt.setAttribute('font-size', '14px'); txt.setAttribute('font-weight', '500');"
            "      txt.textContent = c.message; g.appendChild(txt);"
            "      var sha = document.createElementNS('http://www.w3.org/2000/svg', 'text');"
            "      sha.setAttribute('x', x + 25); sha.setAttribute('y', y + 20); sha.setAttribute('fill', '#475569');"
            "      sha.setAttribute('font-size', '10px'); sha.setAttribute('font-family', 'monospace');"
            "      sha.textContent = c.sha.substring(0,8); g.appendChild(sha);"
            "      c.refs.forEach(function(r, ri) {"
            "        var rt_g = document.createElementNS('http://www.w3.org/2000/svg', 'g');"
            "        var rt_r = document.createElementNS('http://www.w3.org/2000/svg', 'rect');"
            "        var r_text = '[' + r + ']';"
            "        var rt_t = document.createElementNS('http://www.w3.org/2000/svg', 'text');"
            "        rt_t.setAttribute('x', x - 40 - (ri*80)); rt_t.setAttribute('y', y + 4); rt_t.setAttribute('fill', '#4ade80');"
            "        rt_t.setAttribute('font-size', '11px'); rt_t.setAttribute('font-weight', 'bold'); rt_t.setAttribute('font-family', 'monospace');"
            "        rt_t.textContent = r_text; g.appendChild(rt_t);"
            "      });"
            "      svg.appendChild(g);"
            "    });"
            "  });"
            "};"
            "window.checkout = function(t) { fetch('/api/checkout', {method: 'POST', body: 'target=' + t, headers: {'Content-Type': 'application/x-www-form-urlencoded'}}).then(function(){window.loadGraph();}); };"
            "window.newCommit = function() { var m = prompt('Enter snapshot description:'); if(m) fetch('/api/commit', {method: 'POST', body: 'message=' + encodeURIComponent(m), headers: {'Content-Type': 'application/x-www-form-urlencoded'}}).then(function(){window.loadGraph();}); };"
            "window.newBranch = function() { var n = prompt('Enter new branch name:'); if(n) fetch('/api/branch/create', {method: 'POST', body: 'name=' + encodeURIComponent(n), headers: {'Content-Type': 'application/x-www-form-urlencoded'}}).then(function(){ window.loadBranches(); window.loadGraph(); }); };"
            "window.loadAll = function() { window.loadBranches(); window.loadGraph(); };"
            "document.addEventListener('DOMContentLoaded', window.loadAll);"
            "</script></body></html>";
        mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", html);
    } else { mg_http_reply(c, 404, "", "Not Found"); }
  }
}

int pc_serve(int port) {
  struct mg_mgr mgr; char addr[128];
  snprintf(addr, sizeof(addr), s_http_addr, port);
  mg_mgr_init(&mgr);
  if (mg_http_listen(&mgr, addr, fn, NULL) == NULL) return 1;
  printf("\033[1;32mPichrono Visual Dashboard: %s\033[0m\n", addr);
  for (;;) mg_mgr_poll(&mgr, 1000);
  mg_mgr_free(&mgr);
  return 0;
}
