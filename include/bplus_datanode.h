#ifndef BP_DATANODE_H
#define BP_DATANODE_H

#include "record.h"
#include "bf.h"

/*
 * Layout ενός leaf block σύμφωνα με τις διαφάνειες:
 *
 *  -----------------------------------------------------------
 * | DataNodeHeader | Record0 | Record1 | ... | Record(N-1) |
 *  -----------------------------------------------------------
 *
 * Όλα βρίσκονται ΣΤΟ ΙΔΙΟ BF BLOCK.
 */

/* Header ενός leaf node */
typedef struct {
    int is_leaf;       // Πάντα 1 για data nodes
    int num_records;   // Πόσες έγκυρες εγγραφές υπάρχουν
    int next_leaf;     // block_id του επόμενου leaf (-1 αν δεν υπάρχει)
} DataNodeHeader;

/*
 * Υπολογισμός μέγιστων records στο block:
 * (BF_BLOCK_SIZE - sizeof(DataNodeHeader)) / schema->record_size
 *
 * Αυτό ΔΕΝ το κάνουμε εδώ — θα γίνει μέσα στη create/open.
 */

/* Εισαγωγή record σε leaf node, με μετατόπιση */
int data_node_insert_record(
        char* block_data,
        const Record* record,
        const TableSchema* schema,
        int max_records);

/* Πρόσβαση στο N-th record (όχι malloc, direct pointer) */
Record* data_node_get_record(
        char* block_data,
        int index,
        const TableSchema* schema);

/* Γραμμική αναζήτηση στο leaf (επιτρεπτό από άσκηση) */
int data_node_find_record(
        char* block_data,
        int key,
        const TableSchema* schema);

/* Επιστρέφει pointer στο header του block */
static inline DataNodeHeader* data_node_header(char* block_data) {
    return (DataNodeHeader*) block_data;
}

/* Επιστρέφει pointer στο πρώτο record του block */
static inline char* data_node_records_start(char* block_data) {
    return block_data + sizeof(DataNodeHeader);
}

void data_node_print(char* block_data, const TableSchema* schema);

#endif
