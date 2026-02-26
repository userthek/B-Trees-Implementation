#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "bf.h"
#include "record.h"
#include "record_generator.h"
#include "bplus_file_funcs.h"

static void expect_height_split(BPlusMeta *meta, int capacity) {
    if (capacity <= 1) return; // degenerate safety
    assert(meta->height >= 2);
}

int main(void) {
    BF_Init(LRU);

    TableSchema schema = employee_get_schema();
    const char *fname = "test_split_duplicate.bplus";
    remove(fname);

    bplus_create_file(&schema, fname);

    int fd;
    BPlusMeta *meta;
    if (bplus_open_file(fname, &fd, &meta) != 0) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    int capacity = meta->max_records;
    int total = capacity + 2; // force at least one split

    Record r;
    // First insert unique keys
    for (int i = 0; i < total; i++) {
        record_create(&schema, &r, i * 2, "X", "Y", "Z");
        int leaf = bplus_record_insert(fd, meta, &r);
        assert(leaf >= 1);
    }

    // Duplicate insert should fail
    record_create(&schema, &r, 4, "DUP", "Y", "Z");
    int dup_res = bplus_record_insert(fd, meta, &r);
    assert(dup_res == -1);

    // Tree should have grown in height after overflow
    expect_height_split(meta, capacity);

    // Verify all originals still findable
    for (int i = 0; i < total; i++) {
        Record *out = NULL;
        int f = bplus_record_find(fd, meta, i * 2, &out);
        assert(f == 0);
        assert(out != NULL);
        assert(record_get_key(&schema, out) == i * 2);
        free(out);
    }

    // Duplicate key still resolves to single record
    Record *dup_out = NULL;
    int fdup = bplus_record_find(fd, meta, 4, &dup_out);
    assert(fdup == 0);
    assert(dup_out != NULL);
    assert(record_get_key(&schema, dup_out) == 4);
    free(dup_out);

    bplus_close_file(fd, meta);
    BF_Close();
    printf("test_split_duplicate: PASS\n");
    return 0;
}
