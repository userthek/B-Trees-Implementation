#include "bplus_index_node.h"
#include "record.h"
#include <string.h>
#include <stdio.h>

//αρχικοποιούμε header εσωτερικού κόμβου ώστε να δηλώνει index και να είναι κενός
void index_node_init(char* block_data)
{
    IndexNodeHeader* h = (IndexNodeHeader*) block_data;
    h->is_leaf = 0;
    h->num_keys = 0;
}

//βρίσκουμε ταξινομημένη θέση για νέο key, -1 αν υπάρχει ήδη (φραγή διπλότυπου)
static int find_insert_position(char* block_data, int key, int max_keys)
{
    IndexNodeHeader* h = (IndexNodeHeader*) block_data;
    int* keys = index_node_keys(block_data);

    for (int i = 0; i < h->num_keys; i++) {
        if (key == keys[i])
            return -1; //διπλότυπο
        if (key < keys[i])
            return i; //μπαίνει πριν από το πρώτο μεγαλύτερο
    }
    return h->num_keys; //στο τέλος αν είναι το μεγαλύτερο
}

//εισάγουμε προαγμένο key και δεξιό child pointer σε κόμβο index
int index_node_insert_key(
        char* block_data,
        int key,
        int right_child,
        int max_keys)
{
    IndexNodeHeader* h = (IndexNodeHeader*) block_data;
    int* keys = index_node_keys(block_data);
    int* children = index_node_children(block_data, max_keys);

    if (h->num_keys == max_keys)
        return -1; //γεμάτο

    int pos = find_insert_position(block_data, key, max_keys);
    if (pos == -1)
        return -2; //διπλότυπο

    //μετακινούμε κλειδιά και "δείκτες" δεξιά για να κάνουμε χώρο
    for (int i = h->num_keys; i > pos; i--)
        keys[i] = keys[i - 1];
    for (int i = h->num_keys + 1; i > pos + 1; i--)
        children[i] = children[i - 1];

    //εισάγουμε το νέο κλειδί και το δεξιό παιδί στη σωστή θέση
    keys[pos] = key;
    children[pos + 1] = right_child;

    //ενημερώνουμε τα μεταδεδομένα
    h->num_keys++;
    return pos;
}

//κατεβαίνουμε στο παιδί που περιέχει (ή θα έπρεπε να περιέχει) το key
int index_node_find_child(char* block_data, int key, int max_keys)
{
    IndexNodeHeader* h = (IndexNodeHeader*) block_data;
    int* keys = index_node_keys(block_data);
    int* children = index_node_children(block_data, max_keys);

    int i = 0;
    while (i < h->num_keys && key >= keys[i])
        i++;

    return children[i];
}

//εκτυπώνουμε περιεχόμενα ενός index node (κλειδιά και δείκτες παιδιών)
void index_node_print(char* block_data, int max_keys)
{
    IndexNodeHeader* h = (IndexNodeHeader*) block_data;
    int* keys = index_node_keys(block_data);
    int* children = index_node_children(block_data, max_keys);

    printf("IndexNode { keys=%d }\n", h->num_keys);

    printf("  Keys: [ ");
    for (int i = 0; i < h->num_keys; i++)
        printf("%d ", keys[i]);
    printf("]\n");

    printf("  Children: [ ");
    for (int i = 0; i <= h->num_keys; i++)
        printf("%d ", children[i]);
    printf("]\n");
}
