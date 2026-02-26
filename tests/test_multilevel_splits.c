#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "bf.h"
#include "record.h"
#include "record_generator.h"
#include "bplus_file_funcs.h"

int main(void) {
    BF_Init(LRU);

    TableSchema schema = employee_get_schema();
    const char *fname = "test_multilevel_splits.bplus";
    remove(fname);

    bplus_create_file(&schema, fname);

    int fd;
    BPlusMeta *meta;
    if (bplus_open_file(fname, &fd, &meta) != 0) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    int leaf_cap = meta->max_records;
    int idx_cap = meta->max_keys;           // keys per index node
    int leaves_needed = idx_cap + 3;        // force root to split and create level-3
    int total_records = leaves_needed * leaf_cap;

    Record r;
    for (int i = 0; i < total_records; i++) {
        int key = i * 3 + 1; // unique, increasing
        record_create(&schema, &r, key, "A", "B", "C");
        int leaf = bplus_record_insert(fd, meta, &r);
        assert(leaf >= 1);
    }

    // After enough inserts, tree should have grown beyond height 2
    assert(meta->height >= 3);

    // Validate selected keys across the range
    int probe_keys[] = {1, (total_records / 2) * 3 + 1, (total_records - 1) * 3 + 1};
    for (int i = 0; i < 3; i++) {
        Record *out = NULL;
        int found = bplus_record_find(fd, meta, probe_keys[i], &out);
        assert(found == 0);
        assert(out != NULL);
        assert(record_get_key(&schema, out) == probe_keys[i]);
        free(out);
    }

    // Spot check a missing key
    Record *missing = NULL;
    int nf = bplus_record_find(fd, meta, -12345, &missing);
    assert(nf == -1);
    assert(missing == NULL);

    bplus_close_file(fd, meta);
    BF_Close();
    printf("test_multilevel_splits: PASS (height=%d, records=%d)\n", meta->height, total_records);
    return 0;
}
