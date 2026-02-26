//
// Created by theofilos on 11/4/25.
//

#ifndef BPLUS_BPLUS_FILE_STRUCTS_H
#define BPLUS_BPLUS_FILE_STRUCTS_H
#include "bf.h"
#include "bplus_datanode.h"
#include "bplus_index_node.h"
#include "record.h"

/*
 * Layout of block 0:
 * ---------------------------------------------------------
 * | BPlusMeta |  (unused padding space up to BF_BLOCK_SIZE)
 * ---------------------------------------------------------
 *
 * This block is loaded entirely in RAM via BPlusMeta.
 */

typedef struct {

    // -------------------------------
    //  Schema information
    // -------------------------------
    TableSchema schema;      // full schema, copied entirely into metadata

    // -------------------------------
    //  Tree structure
    // -------------------------------
    int root_block;          // block id of root node
    int height;              // height of tree (root = height 1)
    int first_leaf;          // block id of leftmost leaf
    int last_leaf;           // block id of rightmost leaf (optional optimization)

    // -------------------------------
    //  Capacity information
    // -------------------------------
    int max_records;         // max records per leaf block (computed on create/open)
    int max_keys;            // max keys per index block  (computed on create/open)

} BPlusMeta;

#endif

