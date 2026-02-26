#include "../include/record.h"
#include "../include/bplus_file_funcs.h"
#include "../include/record_generator.h"

#include <stdio.h>

int main() {
    BF_Init(LRU);

    TableSchema schema = employee_get_schema();

    const char *fname = "simple_insert.bplus";
    bplus_create_file(&schema, fname);

    int fd;
    BPlusMeta *meta;
    bplus_open_file(fname, &fd, &meta);

    Record r;

    // Insert 3 records (no splits should happen)
    record_create(&schema, &r, 10, "John", "Doe", "Athens");
    bplus_record_insert(fd, meta, &r);

    record_create(&schema, &r, 5, "Alice", "Blue", "Patra");
    bplus_record_insert(fd, meta, &r);

    record_create(&schema, &r, 20, "Nick", "White", "Volos");
    bplus_record_insert(fd, meta, &r);

    printf("Inserted 3 records without split.\n");

    bplus_close_file(fd, meta);
    return 0;
}
