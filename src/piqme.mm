#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "piqme.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

static id<MTLDevice> device = nil;
static id<MTLCommandQueue> commandQueue = nil;
static id<MTLComputePipelineState> pipelineState = nil;

static const char *shaderSource = R"(
#include <metal_stdlib>
using namespace metal;

#define QUARTERROUND(a, b, c, d) \
    a += b; d ^= a; d = (d << 16) | (d >> 16); \
    c += d; b ^= c; b = (b << 12) | (b >> 20); \
    a += b; d ^= a; d = (d << 8) | (d >> 24); \
    c += d; b ^= c; b = (b << 7) | (b >> 25);

kernel void chacha20_encrypt(
    device const uint32_t *input [[buffer(0)]],
    device uint32_t *output [[buffer(1)]],
    device const uint32_t *key [[buffer(2)]],
    device const uint32_t *nonce [[buffer(3)]],
    uint id [[thread_position_in_grid]],
    uint total [[threads_per_grid]]
) {
    if (id * 16 >= total) return;

    uint32_t state[16];
    state[0] = 0x61707865;
    state[1] = 0x3320646e;
    state[2] = 0x79622d32;
    state[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) state[4 + i] = key[i];
    state[12] = id;
    state[13] = nonce[0];
    state[14] = nonce[1];
    state[15] = nonce[2];

    uint32_t initial_state[16];
    for (int i = 0; i < 16; i++) initial_state[i] = state[i];

    for (int i = 0; i < 10; i++) {
        QUARTERROUND(state[0], state[4], state[8],  state[12]);
        QUARTERROUND(state[1], state[5], state[9],  state[13]);
        QUARTERROUND(state[2], state[6], state[10], state[14]);
        QUARTERROUND(state[3], state[7], state[11], state[15]);
        QUARTERROUND(state[0], state[5], state[10], state[15]);
        QUARTERROUND(state[1], state[6], state[11], state[12]);
        QUARTERROUND(state[2], state[7], state[8],  state[13]);
        QUARTERROUND(state[3], state[4], state[9],  state[14]);
    }

    for (int i = 0; i < 16; i++) {
        state[i] += initial_state[i];
        output[id * 16 + i] = input[id * 16 + i] ^ state[i];
    }
}
)";

static void piqme_init_metal() {
    if (device) return;
    device = MTLCreateSystemDefaultDevice();
    if (!device) return;
    commandQueue = [device newCommandQueue];

    NSError *error = nil;
    NSString *source = [NSString stringWithUTF8String:shaderSource];
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if (!library) return;

    id<MTLFunction> function = [library newFunctionWithName:@"chacha20_encrypt"];
    pipelineState = [device newComputePipelineStateWithFunction:function error:&error];
}

static int piqme_process(const char *in_data, size_t in_size, char **out_data, size_t *out_size, bool encrypt) {
    piqme_init_metal();
    if (!pipelineState) return -1;

    // Pad in_size to 64-byte blocks (16 uint32_t)
    size_t padded_size = ((in_size + 63) / 64) * 64;
    uint32_t *padded_in = (uint32_t *)calloc(1, padded_size);
    memcpy(padded_in, in_data, in_size);

    id<MTLBuffer> inBuffer = [device newBufferWithBytes:padded_in length:padded_size options:MTLResourceStorageModeShared];
    id<MTLBuffer> outBuffer = [device newBufferWithLength:padded_size options:MTLResourceStorageModeShared];
    
    // Mock CSIDH-derived key and nonce
    uint32_t key[8] = {0x01234567, 0x89abcdef, 0xfedcba98, 0x76543210, 0xdeadbeef, 0xbaadface, 0x12345678, 0x9abcdef0};
    uint32_t nonce[3] = {0x00000001, 0x00000002, 0x00000003};
    id<MTLBuffer> keyBuffer = [device newBufferWithBytes:key length:sizeof(key) options:MTLResourceStorageModeShared];
    id<MTLBuffer> nonceBuffer = [device newBufferWithBytes:nonce length:sizeof(nonce) options:MTLResourceStorageModeShared];

    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    [encoder setComputePipelineState:pipelineState];
    [encoder setBuffer:inBuffer offset:0 atIndex:0];
    [encoder setBuffer:outBuffer offset:0 atIndex:1];
    [encoder setBuffer:keyBuffer offset:0 atIndex:2];
    [encoder setBuffer:nonceBuffer offset:0 atIndex:3];

    NSUInteger threadsPerThreadgroup = pipelineState.maxTotalThreadsPerThreadgroup;
    if (threadsPerThreadgroup > 64) threadsPerThreadgroup = 64;
    MTLSize threadgroupSize = MTLSizeMake(threadsPerThreadgroup, 1, 1);
    MTLSize threadgroupCount = MTLSizeMake((padded_size / 64 + threadsPerThreadgroup - 1) / threadsPerThreadgroup, 1, 1);

    [encoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadgroupSize];
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    *out_data = (char *)malloc(padded_size);
    memcpy(*out_data, [outBuffer contents], padded_size);
    *out_size = padded_size;

    free(padded_in);
    return 0;
}

int piqme_encrypt(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    char *compressed;
    size_t comp_size;
    if (compress_data(in_data, in_size, &compressed, &comp_size) != 0) return -1;

    char *encrypted;
    size_t enc_padded_size;
    int res = piqme_process(compressed, comp_size, &encrypted, &enc_padded_size, true);
    free(compressed);
    if (res != 0) return -1;

    *out_data = (char *)malloc(sizeof(size_t) + enc_padded_size);
    memcpy(*out_data, &comp_size, sizeof(size_t));
    memcpy(*out_data + sizeof(size_t), encrypted, enc_padded_size);
    *out_size = sizeof(size_t) + enc_padded_size;
    free(encrypted);
    return 0;
}

int piqme_decrypt(const char *in_data, size_t in_size, char **out_data, size_t *out_size) {
    if (in_size < sizeof(size_t)) return -1;

    size_t comp_size;
    memcpy(&comp_size, in_data, sizeof(size_t));

    char *decrypted_padded;
    size_t dec_padded_size;
    if (piqme_process(in_data + sizeof(size_t), in_size - sizeof(size_t), &decrypted_padded, &dec_padded_size, false) != 0) return -1;

    int res = decompress_data(decrypted_padded, comp_size, out_data, out_size);
    free(decrypted_padded);
    return res;
}
