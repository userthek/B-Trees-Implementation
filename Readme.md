# B+ Trees — Database Systems Implementation

> University project for the course **"Database Systems Implementation" (K18)**  
> Winter Semester 2025–2026 · Department of Informatics & Telecommunications, UoA

## Overview

This project implements a complete **B+ Tree file management system** in C, built on top of the **BF (Block File) layer** — a buffer manager that handles low-level disk block access.

The system supports:
- Creating and opening B+ Tree files
- Inserting records in sorted order
- Searching for records by key
- Automatic node splitting to maintain tree balance

---

## Architecture

The implementation is built around four core components:

### 1. Metadata (`BPlusMeta`)
Stored exclusively in **block 0** of every B+ Tree file. Contains:
- The full record schema (field sizes and types)
- The root block number and tree height
- The total number of leaf nodes
- `max_records`: maximum records per leaf block
- `max_keys`: maximum keys per index block

Storing the full schema in metadata makes the system self-contained — any process that opens the file has complete knowledge of the record structure without relying on external variables.

### 2. Leaf Nodes
Each leaf node stores actual database records. Structure:
```
[ Header | Record 0 | Record 1 | ... | Record N ]
```
- **Header** contains: `is_leaf = 1`, record count, and a pointer to the next leaf (linked list)
- Records are stored **sorted by key**
- Insertion (`data_node_insert_record`) uses linear search to find the correct position, then shifts records right
- Record access (`data_node_get_record`) uses direct offset calculation
- When a leaf is full, **`split_leaf`** is called: it creates a new leaf, moves the upper half of records to it, updates the `next_leaf` pointer, and **promotes the first key of the new leaf** to the parent index node

### 3. Index Nodes
Index nodes store only **keys and child pointers** (no full records). Structure:
```
[ Header | keys[] | children[] ]
```
- `children[]` always has **one more element** than `keys[]`
- `index_node_insert_key` inserts a key in sorted position and shifts child pointers accordingly
- `index_node_find_child` traverses the keys to find the correct child block for a given search key
- When an index node is full, **`index_node_split`** applies the classic B+ Tree algorithm: the **middle key is promoted** upward, the left half stays in the original block, and the right half moves to a new block

### 4. Core File Functions

| Function | Description |
|---|---|
| `bplus_create_file` | Creates a new BF file, initializes metadata, creates the first leaf node (also the initial root) |
| `bplus_open_file` | Opens an existing file and loads metadata into memory |
| `bplus_close_file` | Frees memory and closes the file |
| `bplus_record_insert` | Inserts a record into the tree (see below) |
| `bplus_record_find` | Searches for a record by key (see below) |

---

## Key Algorithms

### Insertion — `bplus_record_insert`
Calls the recursive helper `insert_into_node` starting from the root:

1. If the current node is a **leaf** → attempt direct insert or trigger `split_leaf`
2. If the current node is an **index node** → find the correct child and recurse
3. If a split propagates upward → insert the promoted key into the current index node
4. If the index node is also full → split it and propagate further up
5. If the **root splits** → a new root is created, tree height increases, and the new root block is saved in metadata

### Search — `bplus_record_find`
1. Start at the root
2. While in index nodes → follow child pointers using `index_node_find_child`
3. Upon reaching a leaf → linear search for the target key
4. If found → dynamically allocate a new `Record` struct and copy the data (safe to use after the block is unpinned)
5. If not found → return appropriate error code

---

## Project Structure

```
.
├── include/
│   ├── bf.h                    # BF layer interface
│   ├── bplus_file_structs.h    # Core structs (BPlusMeta, headers)
│   ├── bplus_file_funcs.h      # Main file operations
│   ├── bplus_datanode.h        # Leaf node operations
│   ├── bplus_index_node.h      # Index node operations
│   ├── record.h                # Record struct
│   └── record_generator.h      # Test record generation
├── src/
│   ├── bplus_file_funcs.c
│   ├── bplus_datanode.c
│   ├── bplus_index_node.c
│   ├── record.c
│   └── record_generator.c
├── examples/
│   ├── bplus_main.c
│   └── test_bplus_basic.c
├── tests/
│   ├── test_simple_insert.c
│   ├── test_insert_find.c
│   ├── test_leaf_split.c
│   ├── test_multilevel_splits.c
│   ├── test_big_insert.c
│   ├── test_create_open_close.c
│   └── test_split_duplicate.c
├── lib/
│   └── libbf.so                # Pre-compiled BF library
└── Makefile
```

---

## How to Build & Run

```bash
# Build the project
make

# Run the basic example
./bplus_main

# Run a specific test
./test_insert_find
```

> **Requirements:** GCC, Linux/WSL (the BF library `libbf.so` is Linux-only)

---

## Design Notes

- **Linear search in leaf nodes** is justified by the small block size (512 bytes), which limits the number of records per leaf. In production systems with larger blocks (several KB), binary search would improve performance.
- **Index node structure** strictly follows the standard B+ Tree definition: `n` keys → `n+1` child pointers.
- **Split behavior:** leaf splits always promote the *first key of the new leaf* to the parent; index splits promote the *middle key* upward.
- The full schema is stored in metadata so the system is **completely self-contained** — no external configuration needed to open and use a file.
