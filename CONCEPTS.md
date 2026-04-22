# Pichrono: The Inner Workings

Pichrono is modeled after the "Plumbing" layer of Git. This document explains the core concepts and how they are implemented in the C source code.

## 1. Content-Addressable Storage (The Object Database)
Unlike traditional backup systems that store files by name, Pichrono stores data by its **content**.
- When you `add` a file, the engine calculates a **SHA-1 hash** of its contents.
- The first 2 characters of the hash become a directory name in `.pichrono/objects/`, and the remaining 38 become the filename.
- **Benefit**: If you have 10 copies of the same 1GB file, Pichrono only stores it once.

## 2. The Three Object Types
Every file in the `.pichrono/objects` directory is one of three types:
1. **Blobs**: Binary Large Objects. These are just the compressed contents of your files.
2. **Trees**: These are the "directories" of the version control world. A tree object is a simple text file listing the SHAs and names of the blobs (files) it contains.
3. **Commits**: A snapshot of the entire project. It points to a **Tree** (the state of the folder), a **Parent** (the previous commit), and contains the author's message.

## 3. The Index (The Staging Area)
The `.pichrono/index` is a temporary binary file. When you run `pichrono add`, you aren't committing yet; you are updating the Index. The `commit` command then takes whatever is in the Index, packages it into a **Tree** object, and creates a **Commit** object pointing to that tree.

## 4. References & HEAD
- **Branches**: A branch is simply a text file in `.pichrono/refs/heads/` that contains a 40-character SHA-1 hash. Creating a branch is "instant" because it just involves writing 40 bytes to a new file.
- **HEAD**: This is a pointer to the "current" branch. It tells Pichrono which branch should be updated when you make a new commit.

## 5. Merkle Trees & History
Because every commit points to its parent, the entire history forms a **Directed Acyclic Graph (DAG)**.
- The **Visualizer** (`pc_serve`) traverses this graph backwards from every branch head.
- By comparing the Tree SHAs of two commits, the engine can instantly tell if any file in a project has changed without actually reading the files.

## 6. Embedded Web Engine
Pichrono uses the **Mongoose** C library to host a local server directly from the binary. 
- **JSON API**: The server exposes `/api/graph`, which performs a Breadth-First Search (BFS) of the object database to find all commits.
- **SVG Rendering**: The Frontend calculates "lanes" for different branches and draws Bezier curves to show exactly where the code diverged.

## 7. Safety: Reflog & Recovery
- **Reflog**: Every movement of the HEAD is recorded in `.pichrono/logs/HEAD`. This allows you to undo a `checkout` or find a SHA you accidentally moved away from.
- **Object Recovery**: Since commits are permanent once written to the object store, `pichrono recover` can scan the disk for any file that "looks like a commit," even if no branch points to it.
