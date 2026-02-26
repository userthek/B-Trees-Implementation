#include <stdio.h>
#include <stdlib.h>
#include "bf.h"
#include "record.h"
#include "record_generator.h"
#include "bplus_file_funcs.h"
#include "bplus_datanode.h"
#include "bplus_index_node.h"

#ifndef CALL_BF
#define CALL_BF(call) \
    { BF_ErrorCode code = (call); if (code != BF_OK) { BF_PrintError(code); exit(1); } }
#endif

/* --------------------------- PRINT NODE --------------------------- */
void print_block(int fd, int blk, const BPlusMeta *meta) {
    BF_Block *block;
    BF_Block_Init(&block);

    CALL_BF(BF_GetBlock(fd, blk, block));
    char *data = BF_Block_GetData(block);
    int is_leaf = *((int*)data);

    printf("\n===== BLOCK %d =====\n", blk);

    if (is_leaf)
        data_node_print(data, &meta->schema);
    else
        index_node_print(data, meta->max_keys);

    printf("=====================\n");

    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
}

/* Get next leaf */
int next_leaf(int fd, int blk) {
    BF_Block *block;
    BF_Block_Init(&block);

    CALL_BF(BF_GetBlock(fd, blk, block));
    int next = ((DataNodeHeader*)BF_Block_GetData(block))->next_leaf;

    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    return next;
}

/* --------------------------- PRINT WHOLE TREE --------------------------- */
void print_tree(int fd, BPlusMeta *meta) {
    printf("\n\n============== TREE STRUCTURE ==============\n");
    printf("Root block: %d\n", meta->root_block);
    printf("Height: %d\n", meta->height);
    printf("Leaf chain starts at: %d\n", meta->first_leaf);

    /* Print root */
    print_block(fd, meta->root_block, meta);

    /* Print all leaves */
    printf("\n--- Leaf Chain ---\n");
    int leaf = meta->first_leaf;
    while (leaf != -1) {
        print_block(fd, leaf, meta);
        leaf = next_leaf(fd, leaf);
    }
    printf("============================================\n\n");
}

/* --------------------------- MAIN DEMO TEST --------------------------- */
int main() {
    printf("\n===== DEMO: FULL STEP-BY-STEP INSERTS =====\n");
    BF_Init(LRU);

    const char *fname = "test_demo.bplus";
    remove(fname);

    TableSchema schema = employee_get_schema();
    bplus_create_file(&schema, fname);

    int fd;
    BPlusMeta *meta;
    bplus_open_file(fname, &fd, &meta);

    printf("\nFILE INFO\n");
    printf("• record size = %d bytes\n", schema.record_size);
    printf("• max_records per leaf = %d\n", meta->max_records);
    printf("• max_keys per index = %d  (children = max_keys + 1 = %d)\n",
           meta->max_keys, meta->max_keys + 1);

    /* --- INSERT ORDER SAME AS TEACHER --- */
    int keys[] = {4, 123, 162, 200,
                  236, 254, 342, 492,
                  325, 395, 12, 452, 174, 395};
    char *names[] = {"Manos", "Nikos", "Lenos", "Nikos",
                     "Nikos", "Lenos", "Tolis", "Lenos",
                     "Tolis", "Stefi", "Lenos", "Stefi", "Stefi", "Stefi"};

    int total = sizeof(keys) / sizeof(int);

    printf("\n===== START INSERTIONS =====\n");

    for (int i = 0; i < total; i++) {
        Record r;
        record_create(&schema, &r, keys[i], names[i], "X", "Y");

        printf("\n-----------------------------------------------\n");
        printf(" Inserting (%d,%s)\n", keys[i], names[i]);
        printf("-----------------------------------------------\n");

        bplus_record_insert(fd, meta, &r);

        print_tree(fd, meta);
    }

    printf("===== DEMO FINISHED SUCCESSFULLY =====\n");

    bplus_close_file(fd, meta);
    return 0;
}
