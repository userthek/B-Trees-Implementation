#include <stdio.h>
#include <stdlib.h>
#include "bplus_file_funcs.h"
#include "record_generator.h"
#include "bf.h"

int main() {

    BF_Init(LRU);  // Reset BF internal tables with LRU policy

    TableSchema schema = employee_get_schema();
    const char *filename = "test_create.bin";

    remove(filename);  // remove old file if exists

    printf("\n===== TEST: CREATE / OPEN / CLOSE =====\n");

    // create
    if (bplus_create_file(&schema, filename) != 0) {
        printf("FAIL: create\n");
        return 1;
    }

    // open
    int fd;
    BPlusMeta *meta;
    if (bplus_open_file(filename, &fd, &meta) != 0) {
        printf("FAIL: open\n");
        return 1;
    }

    // validate
    if (meta->root_block != 1 || meta->first_leaf != 1) {
        printf("FAIL: metadata values wrong\n");
        return 1;
    }

    printf("Root block = %d\n", meta->root_block);
    printf("Max records per leaf = %d\n", meta->max_records);
    printf("Max keys per index = %d\n", meta->max_keys);

    // cleanup
    bplus_close_file(fd, meta);

    printf("PASS\n");
    return 0;
}
