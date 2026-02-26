#ifndef BP_INDEX_NODE_H
#define BP_INDEX_NODE_H

/*
 * Layout ενός index block σύμφωνα με τις διαφάνειες:
 *
 * ---------------------------------------------------------------
 * | IndexNodeHeader | keys[] | children[]                       |
 * ---------------------------------------------------------------
 *
 * keys[i]      : int keys (sorted)
 * children[i]  : block IDs των παιδιών
 *
 * IMPORTANT:
 *  - το μέγεθος keys[] και children[] δεν ορίζεται εδώ (δεν ξέρουμε max_keys)
 *  - το BF block περιέχει ΟΛΑ αυτά συνεχόμενα στη μνήμη
 *  - το max_keys αποθηκεύεται στο BPlusMeta
 */

/* Header ενός index node */
typedef struct {
    int is_leaf;     // Πάντα 0 για index nodes
    int num_keys;    // Πόσα κλειδιά υπάρχουν αυτή τη στιγμή
} IndexNodeHeader;

/*
 * Επιστρέφει pointer στα keys[] μέσα στο block
 */
static inline int* index_node_keys(char* block_data) {
    return (int*) (block_data + sizeof(IndexNodeHeader));
}

/*
 * Επιστρέφει pointer στα children[] μέσα στο block
 * children array ξεκινά *αμέσως μετά* τα keys
 */
static inline int* index_node_children(char* block_data, int max_keys) {
    char* keys_start = block_data + sizeof(IndexNodeHeader);
    char* children_start = keys_start + max_keys * sizeof(int);
    return (int*) children_start;
}

int index_node_find_child(char* block_data, int key, int max_keys);
void index_node_init(char* block_data);
static int find_insert_position(char* block_data, int key, int max_keys);
int index_node_insert_key(
        char* block_data,
        int key,
        int right_child,        //block ID για εισαγωγή ΜΕΤΑ το κλειδί (δεξιό παιδί)
        int max_keys);

int index_node_find_child(char* block_data, int key, int max_keys);
void index_node_print(char* block_data, int max_keys);


#endif
