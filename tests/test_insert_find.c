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
    const char *fname = "test_insert_find.bplus";
    remove(fname);

    bplus_create_file(&schema, fname);

    int fd;
    BPlusMeta *meta;
    if (bplus_open_file(fname, &fd, &meta) != 0) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    int keys[] = {50, 20, 70, 10, 90, 30, 60, 80, 40};
    int nkeys = sizeof(keys) / sizeof(keys[0]);

    Record r;
    for (int i = 0; i < nkeys; i++) {
        record_create(&schema, &r, keys[i], "A", "B", "C");
        int leaf_id = bplus_record_insert(fd, meta, &r);
        assert(leaf_id >= 1);
    }

    for (int i = 0; i < nkeys; i++) {
        Record *out = NULL;
        int found = bplus_record_find(fd, meta, keys[i], &out);
        assert(found == 0);
        assert(out != NULL);
        assert(record_get_key(&schema, out) == keys[i]);
        free(out);
    }

    Record *missing = NULL;
    int nf = bplus_record_find(fd, meta, 999999, &missing);
    assert(nf == -1);
    assert(missing == NULL);

    bplus_close_file(fd, meta);
    BF_Close();
    printf("test_insert_find: PASS\n");
    return 0;
}
