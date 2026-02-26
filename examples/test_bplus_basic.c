#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "bf.h"
#include "record.h"
#include "bplus_file_funcs.h"
#include "bplus_file_structs.h"
#include "bplus_datanode.h"

void test_create_open()
{
    printf("=== TEST 1: CREATE + OPEN ===\n");

    // 1. Φτιάχνουμε ένα schema (3 πεδία όπως στο παράδειγμα με employees)
    TableSchema schema;
    AttributeSchema attrs[] = {
        {"id", TYPE_INT, sizeof(int)},
        {"name", TYPE_CHAR, 20},
        {"dept", TYPE_CHAR, 20}
    };
    schema_init(&schema, attrs, 3, "id");

    // 2. Δημιουργία B+ αρχείου
    assert(bplus_create_file(&schema, "test_tree.bpt") == 0);

    // 3. Άνοιγμα αρχείου
    int fd;
    BPlusMeta* meta = NULL;

    assert(bplus_open_file("test_tree.bpt", &fd, &meta) == 0);

    // 4. Έλεγχος metadata
    assert(meta != NULL);
    assert(meta->root_block == 1);
    assert(meta->first_leaf == 1);
    assert(meta->last_leaf == 1);
    assert(meta->height == 1);

    // 5. Έλεγχος Block 1 (root leaf)
    BF_Block* blk;
    BF_Block_Init(&blk);

    assert(BF_GetBlock(fd, 1, blk) == BF_OK);

    char* data = BF_Block_GetData(blk);
    DataNodeHeader* h = (DataNodeHeader*)data;

    assert(h->is_leaf == 1);
    assert(h->num_records == 0);
    assert(h->next_leaf == -1);

    printf("Root leaf header OK:\n");
    printf("  is_leaf     = %d\n", h->is_leaf);
    printf("  num_records = %d\n", h->num_records);
    printf("  next_leaf   = %d\n\n", h->next_leaf);

    // Cleanup
    BF_UnpinBlock(blk);
    BF_Block_Destroy(&blk);

    bplus_close_file(fd, meta);

    printf(">>> TEST CREATE + OPEN → SUCCESS ✔\n\n");
}

int main()
{
    BF_Init(LRU);

    test_create_open();

    BF_Close();

    return 0;
}
