#include "bplus_datanode.h"
#include <string.h>
#include <stdio.h>

/*
 * κάθε leaf block έχει στην αρχή ένα DataNodeHeader και αμέσως μετά τα records back-to-back.
 * δεν κρατάμε έξτρα δείκτες: όλα τα offsets υπολογίζονται με απλή αριθμητική πάνω στο block.
 * οι εγγραφές μέσα στο leaf μένουν πάντα ταξινομημένες ως προς το ακέραιο κλειδί (B+ invariant).
 * τα max_records τα ξέρουμε από τα metadata, άρα μπορούμε να κάνουμε απλό linear search/shift.
 */

// ---------------------------------------------------------------------------
// βοηθητική συνάρτηση που υπολογίζει σε ποια μετατόπιση (offset) εντός του
// block βρίσκεται η index-οστή εγγραφή. Σκεπτικό:
//   -> Στην αρχή του block υπάρχει το DataNodeHeader.
//   -> Ακολουθούν σειριακά τα records, καθένα με μέγεθος schema->record_size.
// Άρα το offset = μέγεθος header + (index * μέγεθος εγγραφής).
// χρησιμοποιείται από όλες τις συναρτήσεις για εύρεση της σωστής θέσης.
// ---------------------------------------------------------------------------
static int get_record_offset(int index, const TableSchema* schema) {
    return sizeof(DataNodeHeader) + index * schema->record_size;
}

// ---------------------------------------------------------------------------
// βρίσκει πού πρέπει να εισαχθεί ένα νέο record ώστε να
// διατηρηθεί η ταξινόμηση του leaf. Η αναζήτηση είναι γραμμική επειδή τα
// leaf blocks είναι μικρά και δεν χρειάζεται binary search.
//
// Επιστρέφει:
//   - θέση (index) όπου πρέπει να μπει το record
//   - -1 αν υπάρχει ήδη record με το ίδιο key (δηλαδή διπλότυπο)
//
// με λογική :
//   - σαρώνουμε τις υπάρχουσες εγγραφές από την αρχή.
//   - αν το νέο κλειδί είναι μικρότερο από κάποιο υπάρχον, μπαίνει πριν.
//   - αν τελειώσει η σάρωση, σημαίνει ότι μπαίνει στο τέλος.
//   - αν το νέο κλειδί είναι ίδιο με υπάρχον, δεν κάνουμε εισαγωγή.
// ---------------------------------------------------------------------------
static int find_insert_position(char* block_data, const TableSchema* schema, int key) {
    DataNodeHeader* h = (DataNodeHeader*) block_data;

    for (int i = 0; i < h->num_records; i++) {
        Record* rec = data_node_get_record(block_data, i, schema);
        int existing_key = record_get_key(schema, rec);

        if (key < existing_key)
            return i;   //μπαίνει πριν από το πρώτο μεγαλύτερο
        if (key == existing_key)
            return -1; //διπλότυπο
    }

    return h->num_records; //στο τέλος αν είναι το μεγαλύτερο
}

/// ---------------------------------------------------------------------------
//εισαγωγή νέας εγγραφής σε leaf node, κρατώντας την ταξινόμηση. 
//σκεπτικό:
//   1. Ελεγχος αν το block είναι γεμάτο.
//   2. Βρίσκουμε σε ποια θέση πρέπει να μπει το νέο record.
//   3. Αν χρειάζεται, μετακινούμε όλες τις εγγραφές δεξιά (memcpy shift).
//   4. Αντιγράφουμε την καινούρια εγγραφή στη θέση που της αναλογεί.
//   5. Αυξάνουμε τον μετρητή των εγγραφών.
//
// Επιστροφές:
//   - θέση (index) όπου έγινε η εισαγωγή
//   - -1 αν δεν υπάρχει χώρος
//   - -2 αν υπάρχει διπλότυπο key
// ---------------------------------------------------------------------------
int data_node_insert_record(
        char* block_data,
        const Record* record,
        const TableSchema* schema,
        int max_records)
{
    DataNodeHeader* h = (DataNodeHeader*)block_data;

    if (h->num_records >= max_records)
        return -1; //γεμάτο block

    int key = record_get_key(schema, record);

    // βρίσκουμε σε ποιο σημείο πρέπει να μπει το νέο record (ή αν είναι διπλότυπο)
    int pos = find_insert_position(block_data, schema, key);
    if (pos == -1)
        return -2; //διπλότυπο

    // κάνουμε χώρο: μετακινούμε τα υπαρκτά bytes ένα slot δεξιά από το τέλος προς τα πίσω
    for (int i = h->num_records; i > pos; i--) {
        int dst = get_record_offset(i, schema);
        int src = get_record_offset(i-1, schema);
        memcpy(block_data + dst, block_data + src, schema->record_size);
    }

    // γράφουμε το νέο record στη σωστή θέση
    int offset = get_record_offset(pos, schema);
    memcpy(block_data + offset, record, schema->record_size);

    //ενημερώνουμε τα μεταδεδομένα
    h->num_records++;
    return pos;
}

// ---------------------------------------------------------------------------
// επστροφη pointer στην index-οστή εγγραφή ενός leaf block.
//
// λογική:
//   - επιβαιβέωση ότι το index είναι έγκυρο.
//   - υπολογισμός το offset με βάση το header και το record_size.
//   - επιστροφή pointer εκεί.
//
// *δεν γίνεται αντιγραφή δεδομένων, απλώς δίνουμε pointer μέσα στο block.
// ---------------------------------------------------------------------------
Record* data_node_get_record(
        char* block_data,
        int index,
        const TableSchema* schema)
{
    DataNodeHeader* h = (DataNodeHeader*)block_data;

    if (index < 0 || index >= h->num_records)
        return NULL;

    int offset = get_record_offset(index, schema);
    return (Record*)(block_data + offset);
}

// ---------------------------------------------------------------------------
// απλή γραμμική αναζήτηση μέσα στο leaf για ένα συγκεκριμένο key.
// επιστρέφει:
//   - το index της εγγραφής αν βρέθηκε
//   - -1 αν δεν υπάρχει
//
// *η αναζήτηση είναι γραμμική γιατί τα leaf blocks είναι μικρά και οι
// εισαγωγές/μετακινήσεις απλές -> δεν χρειάζεται binary search.
// --------------------------------------------------------------------------
int data_node_find_record(char* block_data, int key,
                          const TableSchema* schema)
{
    DataNodeHeader* h = (DataNodeHeader*)block_data;

    for (int i = 0; i < h->num_records; i++) {
        Record* rec = data_node_get_record(block_data, i, schema);
        if (record_get_key(schema, rec) == key)
            return i;
    }

    return -1;
}

//εκτυπώνουμε header και όλες τις εγγραφές ενός leaf για debugging
void data_node_print(char* block_data, const TableSchema* schema)
{
    DataNodeHeader* h = (DataNodeHeader*) block_data;

    printf("LeafNode { records=%d, next=%d }\n",
           h->num_records, h->next_leaf);

    for (int i = 0; i < h->num_records; i++) {
        Record* r = data_node_get_record(block_data, i, schema);
        printf("  [%d] ", i);
        record_print(schema, r);
    }
}
