# Pichrono

**Pichrono** is a high-performance, low-weight version control system written entirely in C. It implements the fundamental data structures of Git—including content-addressable storage, Merkle trees, and a Directed Acyclic Graph (DAG) for history—providing a robust engine for tracking code changes with mathematical precision.

## Key Features

- **Compressed Storage**: Every file, tree, and commit is stored using ZLIB compression to minimize disk footprint.
- **Content-Addressing**: Uses SHA-1 hashing to ensure data integrity and deduplication (identical files are only stored once).
- **Branch Management**: Lightweight branching system allowing for parallel development and instant context switching.
- **Visual Dashboard**: An embedded C-based web server (Mongoose) providing a professional SVG-rendered commit tree.
- **Doomsday Recovery**: Built-in `reflog` and an aggressive object scanner to recover "lost" work that isn't pointed to by any branch.

## Installation

### Prerequisites
- GCC Compiler
- ZLIB Development Headers

### Build
```bash
make
```

## Quick Start

1. **Initialize**: `./bin/pichrono init`
2. **Add Files**: `./bin/pichrono add <filename>`
3. **Commit**: `./bin/pichrono commit -m "Your message"`
4. **Branch**: `./bin/pichrono branch feature-alpha`
5. **Visualize**: `make serve` (Open http://localhost:8080)

---
Developed and maintained by
 **Piyush Parashar**
