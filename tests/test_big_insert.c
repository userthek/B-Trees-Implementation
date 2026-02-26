#include <stdio.h>
#include <stdlib.h>

#include "bf.h"
#include "record.h"
#include "record_generator.h"
#include "bplus_file_funcs.h"
#include "bplus_datanode.h"
#include "bplus_index_node.h"

FILE *OUT;

#define CALL_BF_SAFE(call) \
    { BF_ErrorCode code = call; if (code != BF_OK) { BF_PrintError(code); exit(1);} }

void print_block(int fd, int block_id, const BPlusMeta *meta) {
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF_SAFE(BF_GetBlock(fd, block_id, block));
    char *data = BF_Block_GetData(block);

    int is_leaf = *((int*)data);

    fprintf(OUT, "\n===== BLOCK %d =====\n", block_id);

    if (is_leaf)
        data_node_print(data, &meta->schema);
    else
        index_node_print(data, meta->max_keys);

    fprintf(OUT, "========================\n");

    CALL_BF_SAFE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
}

void print_all_blocks(int fd, const BPlusMeta *meta) {
    int blocks;
    BF_GetBlockCounter(fd, &blocks);

    fprintf(OUT, "\n=============================\n");
    fprintf(OUT, " PRINTING ENTIRE FILE (%d blocks)\n", blocks);
    fprintf(OUT, "=============================\n");

    for (int b = 1; b < blocks; b++)
        print_block(fd, b, meta);
}

int main() {
    BF_Init(LRU);

    OUT = fopen("test_big_output.txt", "w");
    if (!OUT) { perror("fopen"); exit(1); }

    fprintf(OUT, "\n===== TEST: ROOT + INDEX SPLIT =====\n");

    const char *fname = "test_big.bplus";
    remove(fname);

    TableSchema schema = employee_get_schema();
    bplus_create_file(&schema, fname);

    int fd;
    BPlusMeta *meta;
    bplus_open_file(fname, &fd, &meta);

    fprintf(OUT, "FILE INFO:\n");
    fprintf(OUT, "• record size = %d bytes\n", schema.record_size);
    fprintf(OUT, "• max_records per leaf = %d\n", meta->max_records);
    fprintf(OUT, "• max_keys per index = %d\n", meta->max_keys);

    int total_inserts = 100;

    for (int i = 1; i <= total_inserts; i++) {
        Record r;
        record_create(&schema, &r, i * 10, "A", "B", "C");
        bplus_record_insert(fd, meta, &r);

        if (i % 10 == 0) {
            fprintf(OUT, "Inserted %d records… current height = %d\n",
                    i, meta->height);
        }
    }

    fprintf(OUT, "\nFINAL HEIGHT = %d\n", meta->height);
    print_all_blocks(fd, meta);

    fprintf(OUT, "\nPASS: Big insert test completed.\n");
    fclose(OUT);

    bplus_close_file(fd, meta);
    return 0;
}
