#include "bplus_index_node.h"
#include "bplus_datanode.h"
#include "bplus_file_funcs.h"
#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#ifndef bplus_ERROR
#define bplus_ERROR -1
#endif
#ifndef bplus_OK
#define bplus_OK 0
#endif

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return bplus_ERROR;     \
    }                         \
  }

//υπολογίζουμε πόσες εγγραφές χωράει ένα leaf block με βάση το schema
static int compute_max_records(const TableSchema* schema) {
    return (BF_BLOCK_SIZE - sizeof(DataNodeHeader)) / schema->record_size;
}

//υπολογίζουμε πόσα κλειδιά χωράει ένα index block
static int compute_max_keys() {
    int available_bytes = BF_BLOCK_SIZE - sizeof(IndexNodeHeader);
    int available_ints = available_bytes / sizeof(int);
    return (available_ints - 1) / 2; //παιδιά=keys+1
}

// ---------------------------------------------------------------------------
// γράφει τα ενημερωμένα metadata πίσω στο block 0 του αρχείου.
//
// σκεπτικό:
//   -> τα metadata B+ tree βρίσκονται πάντα στο block 0.
//   -> όταν αλλάζει κάτι (π.χ. root_block, ύψος, first_leaf), ενημερώνουμε.
//   -> απλό memcpy στο block followed by BF_Block_SetDirty.
//
// πολύ σημαντική λειτουργία για τη διατήρηση της σωστής κατάστασης του δέντρου.
// ---------------------------------------------------------------------------
static int write_metadata(int file_desc, const BPlusMeta* meta) {
    BF_Block* block;
    BF_Block_Init(&block);

    if (BF_GetBlock(file_desc, 0, block) != BF_OK) {
        BF_Block_Destroy(&block);
        return -1;
    }

    char* data = BF_Block_GetData(block);
    memcpy(data, meta, sizeof(BPlusMeta));

    BF_Block_SetDirty(block);
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);

    return 0;
}


// ---------------------------------------------------------------------------
// δημιουργεί νέο αρχείο B+ tree.
// κάνει τα εξής βήματα:
//   1) δημιουργία αρχείου BF και άνοιγμα.
//   2) allocation block 0 -> γράφουμε metadata (schema, root, leaf info).
//   3) allocation πρώτου leaf block που γίνεται και root (ύψος = 1).
//   4) γράφουμε DataNodeHeader στο root leaf (is_leaf = 1, num_records = 0).
//   5) unpin + close.
//
// στο τέλος έχουμε ένα έτοιμο B+ tree με ένα μόνο κενό leaf.
// ---------------------------------------------------------------------------
int bplus_create_file(const TableSchema *schema, const char *fileName) {
    CALL_BF(BF_CreateFile(fileName));
    int fd;
    CALL_BF(BF_OpenFile(fileName, &fd));

    BF_Block *meta_block;
    BF_Block_Init(&meta_block);
    CALL_BF(BF_AllocateBlock(fd, meta_block));

    //αρχικοποίηση metadata
    BPlusMeta meta;
    meta.schema = *schema;
    meta.root_block = 1;
    meta.height = 1;
    meta.first_leaf = 1;
    meta.last_leaf = 1;
    meta.max_records = compute_max_records(schema);
	meta.max_keys = compute_max_keys();

    char *meta_data = BF_Block_GetData(meta_block);
    memcpy(meta_data, &meta, sizeof(BPlusMeta));
    BF_Block_SetDirty(meta_block);

    //αρχικοποίηση root leaf block
    BF_Block *root_block;
    BF_Block_Init(&root_block);
    CALL_BF(BF_AllocateBlock(fd, root_block));
    char *root_data = BF_Block_GetData(root_block);

    DataNodeHeader h;
    h.is_leaf = 1;
    h.num_records = 0;
    h.next_leaf = -1;

    memcpy(root_data, &h, sizeof(DataNodeHeader));
    BF_Block_SetDirty(root_block);

    CALL_BF(BF_UnpinBlock(meta_block));
    CALL_BF(BF_UnpinBlock(root_block));
    BF_Block_Destroy(&meta_block);
    BF_Block_Destroy(&root_block);
    CALL_BF(BF_CloseFile(fd));

    fprintf(stderr, "Created B+ tree file '%s'\n", fileName);
    return 0;
}


// ---------------------------------------------------------------------------
// ανοίγει υπάρχον B+ tree αρχείο και φορτώνει metadata στη μνήμη.
//
// βήματα:
//   -> ανοίγουμε BF αρχείο
//   -> διαβάζουμε block 0
//   -> κάνουμε malloc ένα BPlusMeta struct
//   -> αντιγράφουμε τα metadata από το block στο struct
//
// ο caller παίρνει την ευθύνη να κάνει free() αυτά τα metadata στο close.
// ---------------------------------------------------------------------------
int bplus_open_file(const char *fileName, int *file_desc, BPlusMeta **metadata)
{
    CALL_BF(BF_OpenFile(fileName, file_desc));

    BF_Block *meta_block;
    BF_Block_Init(&meta_block);

    CALL_BF(BF_GetBlock(*file_desc, 0, meta_block));

    *metadata = malloc(sizeof(BPlusMeta));
    if (*metadata == NULL)
    {
        CALL_BF(BF_UnpinBlock(meta_block));
        BF_Block_Destroy(&meta_block);
        return bplus_ERROR;
    }

    char *data = BF_Block_GetData(meta_block);
    memcpy(*metadata, data, sizeof(BPlusMeta));

    CALL_BF(BF_UnpinBlock(meta_block));
    BF_Block_Destroy(&meta_block);

    return bplus_OK;
}


//κλείνουμε αρχείο και απελευθερώνουμε metadata
int bplus_close_file(const int file_desc, BPlusMeta* metadata)
{
    if (metadata != NULL)
        free(metadata);

    BF_ErrorCode code = BF_CloseFile(file_desc);
    if (code != BF_OK) {
        BF_PrintError(code);
        return bplus_ERROR;
    }

    return bplus_OK;
}

//αποτέλεσμα διάσπασης leaf
typedef struct {
    int new_key;        //προαγμένο νέο κλειδί
    int right_block;    //block ID δεξιού νέου leaf
    int inserted_block; //block ID όπου έγινε η εισαγωγή
} SplitResult;

//μετα-πληροφορία split που ανεβαίνει προς τα πάνω μετά από διάσπαση παιδιού
typedef struct {
    int happened;       //αν έγινε split (1) ή όχι (0)
    int promoted_key;   //προαγμένο κλειδί
    int right_block;    //block ID δεξιού νέου κόμβου (leaf ή index)
} ParentSplit;

// ---------------------------------------------------------------------------
// split ενός γεμάτου leaf:
//   1. Το leaf είναι γεμάτο και πρέπει να χωριστεί σε δύο.
//   2. Δημιουργούμε νέο leaf block και μοιράζουμε τις εγγραφές 50-50.
//   3. Ενημερώνουμε την αλυσίδα leaf nodes (next_leaf).
//   4. Εισάγουμε τη νέα εγγραφή είτε στο αριστερό είτε στο δεξί leaf.
//   5. Το πρώτο key του δεξιού leaf “ανεβαίνει” στον parent.
//
// επιστρέφουμε new_block_id και γεμίζουμε το struct out με:
//   -> new_key      (προαγόμενο key)
//   -> right_block  (id του νέου leaf)
//   -> inserted_block (πού μπήκε η νέα εγγραφή)
// ---------------------------------------------------------------------------
static int split_leaf(
        int file_desc,
        BPlusMeta *meta,
        int old_block_id,
        const Record *new_record,
        SplitResult *out)
{
    BF_Block *old_block;
    BF_Block_Init(&old_block);
    CALL_BF(BF_GetBlock(file_desc, old_block_id, old_block));
    char *old_data = BF_Block_GetData(old_block);

    DataNodeHeader *old_h = (DataNodeHeader*) old_data;
    int rec_size = meta->schema.record_size;

    //pointer στην πρώτη εγγραφή του παλιού leaf
    char *old_rec = old_data + sizeof(DataNodeHeader);

    int N = old_h->num_records;

    int new_key = record_get_key(&meta->schema, new_record);

    //έλεγχος για διπλότυπο
    for (int i = 0; i < N; i++) {
        Record *r = data_node_get_record(old_data, i, &meta->schema);
        if (record_get_key(&meta->schema, r) == new_key) {
            fprintf(stderr, "Error: Duplicate key %d (split_leaf)\n", new_key);

            CALL_BF(BF_UnpinBlock(old_block));
            BF_Block_Destroy(&old_block);
            return -1;
        }
    }

    //μοίρασμα εγγραφών
    //αριστερό φύλλο: πρώτα (floor(N/2))
    //δεξί φύλλο: υπόλοιπα (ceil(N/2))
    int mid = N / 2;
    if (mid < 1) mid = 1;

    //δημιουργία νέου leaf block (δεξιά του παλιού)
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(file_desc, new_block));
    char *new_data = BF_Block_GetData(new_block);

    int new_block_id;
    CALL_BF(BF_GetBlockCounter(file_desc, &new_block_id));
    new_block_id--;

    //αρχικοποίηση μεταδεδομένων νέου leaf και ενημέρωση αλυσίδας
    DataNodeHeader *new_h = (DataNodeHeader*) new_data;
    new_h->is_leaf = 1;
    new_h->num_records = 0;
    new_h->next_leaf = old_h->next_leaf;

    old_h->next_leaf = new_block_id;

    //new_rec: pointer στην πρώτη εγγραφή του νέου leaf
    char *new_rec = new_data + sizeof(DataNodeHeader);

    //μεταφορά άνω μισού των εγγραφών στο νέο leaf
    for (int i = mid; i < N; i++) {
        memcpy(new_rec + new_h->num_records * rec_size,
               old_rec + i * rec_size,
               rec_size);
        new_h->num_records++;
    }

    //ενημέρωση παλιού leaf (αφαιρούμε τις μεταφερθείσες εγγραφές)
    old_h->num_records = mid;

    //εισαγωγή της νέας εγγραφής στο σωστό leaf
    int left_last_key;
    {
        Record left_last;
        memcpy(&left_last, old_rec + (mid - 1) * rec_size, rec_size);
        left_last_key = record_get_key(&meta->schema, &left_last);
    }

    if (new_key < left_last_key) {
        //εισάγουμε στο παλιό leaf αν το κλειδί είναι μικρότερο από το μεγαλύτερο αριστερό
        data_node_insert_record(old_data, new_record, &meta->schema, meta->max_records);
        out->inserted_block = old_block_id;
    } else {
        //εισάγουμε στο νέο leaf αν το κλειδί είναι μεγαλύτερο από το μεγαλύτερο αριστερό
        data_node_insert_record(new_data, new_record, &meta->schema, meta->max_records);
        out->inserted_block = new_block_id;
    }

    //ενημέρωση metadata B+ tree (first_leaf, last_leaf)
    if (meta->last_leaf == old_block_id)
        meta->last_leaf = new_block_id;

    int old_first_key;
    {
        Record first;
        memcpy(&first, old_rec, rec_size);
        old_first_key = record_get_key(&meta->schema, &first);
    }

    Record new_first_record;
    memcpy(&new_first_record, new_rec, rec_size);
    int new_first_key = record_get_key(&meta->schema, &new_first_record);

    //ενημέρωση metadata στο αρχείο
    write_metadata(file_desc, meta);

    //προαγόμενο κλειδί και δεξιό νέο block για επιστροφή προς τα πάνω
    Record *first_new = (Record*) new_rec;
    out->new_key = record_get_key(&meta->schema, first_new);
    out->right_block = new_block_id;

    BF_Block_SetDirty(old_block);
    BF_Block_SetDirty(new_block);

    CALL_BF(BF_UnpinBlock(old_block));
    CALL_BF(BF_UnpinBlock(new_block));

    BF_Block_Destroy(&old_block);
    BF_Block_Destroy(&new_block);

    return new_block_id;
}


// ---------------------------------------------------------------------------
//   Ένα internal node γέμισε επειδή ένα από τα παιδιά του έκανε split,
//   οπότε μας έστειλε:
//       -> promoted_key : κλειδί που πρέπει να εισαχθεί στο index node
//       -> right_block  : νέο block από το split του παιδιού
//
//   Το index node όμως είναι ήδη γεμάτο → χρειάζεται split κι αυτό.
//
// Βήματα:
//   1) Φορτώνουμε όλα τα keys + children του node σε προσωρινούς πίνακες
//      (tk, tc), ώστε να κάνουμε εύκολα insert του promoted_key στο σωστό pos.
//   2) Μετά την εισαγωγή, τα δεδομένα tk/tc έχουν μια θέση παραπάνω.
//   3) Υπολογίζουμε mid = total_keys / 2 → το μεσαίο key προάγεται προς τα πάνω.
//   4) Το αριστερό μισό μένει στο old_block, το δεξί μισό πάει στο new_block.
//   5) Επιστρέφουμε στο parent ότι έγινε split, και το middle_key που πρέπει
//      να εισαχθεί εκεί.
//
// Επιστρέφει:
//   -> out->promoted_key : το key που ανεβαίνει στον πατέρα
//   -> out->right_block  : το νέο block
//   -> out->happened     : flag ότι έγινε split
// ---------------------------------------------------------------------------
static int split_index_node(
        int file_desc,
        BPlusMeta *meta,
        int old_block_id,
        ParentSplit child_info,
        ParentSplit *out)
{
    BF_Block *old_block;
    BF_Block_Init(&old_block);
    CALL_BF(BF_GetBlock(file_desc, old_block_id, old_block));
    char *old_data = BF_Block_GetData(old_block);

    IndexNodeHeader *old_h = (IndexNodeHeader*)old_data;

    //αντιγράφουμε κλειδιά και "δείκτες" παιδιών σε προσωρινά arrays
    int *old_keys = index_node_keys(old_data);
    int *old_children = index_node_children(old_data, meta->max_keys);

    int maxk = meta->max_keys;
    int tk[maxk + 1];
    int tc[maxk + 2];

    for (int i = 0; i < old_h->num_keys; i++)
        tk[i] = old_keys[i];
    for (int i = 0; i <= old_h->num_keys; i++)
        tc[i] = old_children[i];

    //βρίσκουμε θέση για το προαγμένο κλειδί
    int pos = 0;
    while (pos < old_h->num_keys && child_info.promoted_key > tk[pos])
        pos++;

    //μετακινούμε κλειδιά και "δείκτες" δεξιά για να κάνουμε χώρο για το νέο κλειδί και "δείκτη"
    for (int i = old_h->num_keys; i > pos; i--)
        tk[i] = tk[i - 1];
    for (int i = old_h->num_keys + 1; i > pos + 1; i--)
        tc[i] = tc[i - 1];

    //εισάγουμε το νέο κλειδί και "δείκτη"
    tk[pos] = child_info.promoted_key;
    tc[pos + 1] = child_info.right_block;

    int total_keys = old_h->num_keys + 1;

    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(file_desc, new_block));
    char *new_data = BF_Block_GetData(new_block);

    int new_block_id;
    CALL_BF(BF_GetBlockCounter(file_desc, &new_block_id));
    new_block_id--;

    //βρίσκουμε μέσο για το split
    int mid = total_keys / 2;

    //το μεσαίο κλειδί προάγεται προς τα πάνω
    int middle_key = tk[mid];

    //ενημερώνουμε παλιό block (αριστερό) με τα κάτω μισά κλειδιά και "δείκτες"
    old_h->num_keys = mid;
    for (int i = 0; i < mid; i++)
        old_keys[i] = tk[i];
    for (int i = 0; i <= mid; i++)
        old_children[i] = tc[i];

    //αρχικοποίηση μεταδεδομένων νέου block (δεξιού)
    IndexNodeHeader *new_h = (IndexNodeHeader*)new_data;
    new_h->is_leaf = 0;
    new_h->num_keys = total_keys - mid - 1;

    //δείκτες σε κλειδιά και "δείκτες" παιδιών του νέου block
    int *new_keys = index_node_keys(new_data);
    int *new_children = index_node_children(new_data, meta->max_keys);

    //ενημέρωση νέου block με τα πάνω μισά κλειδιά και "δείκτες" χωρίς το προαγμένο κλειδί
    for (int i = 0; i < new_h->num_keys; i++)
        new_keys[i] = tk[mid + 1 + i];
    for (int i = 0; i <= new_h->num_keys; i++)
        new_children[i] = tc[mid + 1 + i];

    //ενημέρωση αποτελέσματος split προς τα πάνω
    out->happened     = 1;              //έγινε split
    out->promoted_key = middle_key;     //προαγμένο κλειδί (μεσαίο)
    out->right_block  = new_block_id;   //δεξιό νέο block

    BF_Block_SetDirty(old_block);
    BF_Block_SetDirty(new_block);

    // if (tk != temp_keys) free(tk);
    // if (tc != temp_children) free(tc);

    CALL_BF(BF_UnpinBlock(old_block));
    CALL_BF(BF_UnpinBlock(new_block));

    BF_Block_Destroy(&old_block);
    BF_Block_Destroy(&new_block);

    return 0;
}

// ---------------------------------------------------------------------------
//   Η εισαγωγή σε B+ tree γίνεται πάντα ξεκινώντας από τη ρίζα.
//   Κάθε κόμβος αποφασίζει:
//       1) Αν είναι leaf → απλή εισαγωγή ή split_leaf.
//       2) Αν είναι index → βρες κατάλληλο child και κάλεσε αναδρομή.
//   Μετά την αναδρομή:
//       -> αν το παιδί έκανε split, πρέπει να εισάγουμε promoted_key στον index node
//       -> αν ο index node είναι γεμάτος, split_index_node.
//   - Αν block_id είναι leaf:
//         -> προσπαθούμε απλή εισαγωγή
//         -> αν γεμάτο → split_leaf
//         -> αν διπλότυπο → error
//
//   - Αν block_id είναι index node:
//         -> βρίσκουμε next_child (κανόνας B+ δέντρου)
//         -> recursive insert στο παιδί
//         -> αν child_split.happened = 1 → εισάγουμε promoted_key στον index node
//         -> αν index node γεμάτο → split_index_node
//
// Πληροφορία προς τα πάνω:
//   -> result->happened = 1 όταν ο current κόμβος διασπάται
//   -> αλλιώς = 0, και η εισαγωγή ολοκληρώθηκε τοπικά.
//
// inserted_leaf:
//   -> δείχνει σε ποιο leaf κατέληξε το record μετά από insert/split.
// ---------------------------------------------------------------------------
static int insert_into_node(
        int file_desc,
        BPlusMeta *meta,
        int block_id,
        const Record *record,
        ParentSplit *result,
        int *inserted_leaf)
{
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(file_desc, block_id, block));
    char *data = BF_Block_GetData(block);

    int is_leaf = *((int*)data);

    //base case αναδρομής: προσπαθούμε να εισάγουμε σε leaf
    if (is_leaf == 1) {

        DataNodeHeader *h = (DataNodeHeader*)data;

        //αν υπάρχει χώρος στο leaf, εισάγουμε απευθείας
        if (h->num_records < meta->max_records) {

            int insert_pos = data_node_insert_record(data, record, &meta->schema, meta->max_records);

            //έλεγχος για διπλότυπο
            if (insert_pos == -2) {
                CALL_BF(BF_UnpinBlock(block));
                BF_Block_Destroy(&block);
                result->happened = 0;
                return -1;
            }

            BF_Block_SetDirty(block);

            //ενημερώνουμε το id του leaf block όπου έγινε η εισαγωγή
            if (inserted_leaf)
                *inserted_leaf = block_id;

            CALL_BF(BF_UnpinBlock(block));
            BF_Block_Destroy(&block);

            result->happened = 0;           //δεν έγινε split
            return 0;
        }

        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);

        //αν δεν υπάρχει χώρος, κάνουμε split στο leaf
        SplitResult s;
        int split_ret = split_leaf(file_desc, meta, block_id, record, &s);

        //το split απέτυχε
        if (split_ret == -1) {
            result->happened = 0;
            return -1;
        }

        //ενημερώνουμε το id του leaf block όπου έγινε η εισαγωγή
        if (inserted_leaf)
            *inserted_leaf = s.inserted_block;

        result->happened     = 1;              //έγινε split
        result->promoted_key = s.new_key;      //προαγμένο κλειδί (πρώτο κλειδί νέου leaf block)
        result->right_block  = s.right_block;  //δεξιό νέο leaf block

        return 0;
    }

    //recursive case: είμαστε σε index node

    //ξεκινάμε κατέβασμα στο κατάλληλο παιδί
    int key = record_get_key(&meta->schema, record);

    int next_child = index_node_find_child(data, key, meta->max_keys);

    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    ParentSplit child_split = {0};

    //κατεβαίνουμε προσπαθώντας να εισάγουμε στο παιδί αναδρομικά (μέχρι να φτάσουμε σε leaf)
    int child_status = insert_into_node(file_desc, meta, next_child, record, &child_split, inserted_leaf);

    //απέτυχε η εισαγωγή στο παιδί
    if (child_status == -1) {
        result->happened = 0;
        return -1;
    }

    //αν δεν έγινε split στο παιδί, τελειώνουμε
    if (!child_split.happened) {
        result->happened = 0;
        return 0;
    }

    //αν έγινε split στο παιδί, προσπαθούμε να εισάγουμε το προαγμένο κλειδί στο τρέχον index node
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(file_desc, block_id, block));
    data = BF_Block_GetData(block);

    IndexNodeHeader *h = (IndexNodeHeader*)data;

    //αν υπάρχει χώρος, εισάγουμε απευθείας
    if (h->num_keys < meta->max_keys) {

        int ok = index_node_insert_key(
            data,
            child_split.promoted_key,
            child_split.right_block,
            meta->max_keys
        );

        if (ok < 0) {
                fprintf(stderr, "ERROR: index_node_insert_key failed unexpectedly");
        }

        BF_Block_SetDirty(block);
        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);

        result->happened = 0;
        return 0;
    }

    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    ParentSplit parent_split;

    //αν δεν υπάρχει χώρος, κάνουμε split στο index node
    //και επιστρέφουμε πληροφορία προς τα πάνω
    split_index_node(file_desc, meta, block_id, child_split, &parent_split);

    *result = parent_split;
    return 0;
}

// ---------------------------------------------------------------------------
//   - Όλες οι εισαγωγές περνάνε από εδώ.
//   - Καλούμε insert_into_node ξεκινώντας από meta->root_block.
//   - Υπάρχει μία και μοναδική ειδική περίπτωση:
//         Αν η ρίζα κάνει split → τότε πρέπει να δημιουργηθεί νέα root.
//         Αυτό αυξάνει το ύψος του δέντρου κατά 1.
//   1) Καλούμε insert_into_node.
//   2) Αν δεν έγινε split στη ρίζα → απλή εισαγωγή, επιστροφή leaf id.
//   3) Αν έγινε split:
//          - δημιουργούμε νέο root block
//          - γράφουμε promoted_key
//          - children[0] = παλιά root
//          - children[1] = νέο block από split
//          - ενημερώνουμε metadata (root_block + height).
//
// Επιστρέφουμε:
//   -> inserted_leaf : block id του leaf όπου μπήκε η εγγραφή
//   -> -1 σε περίπτωση error (π.χ. duplicate)
// ---------------------------------------------------------------------------
int bplus_record_insert(int file_desc, BPlusMeta *meta, const Record *record)
{
    ParentSplit root_split = {0};
    int inserted_leaf = -1;

    //καλούμε αναδρομική εισαγωγή από τη ρίζα
    int status = insert_into_node(file_desc, meta, meta->root_block, record, &root_split, &inserted_leaf);

    //η εισαγωγή απέτυχε
    if (status == -1) {
        return -1;
    }

    //αν δεν έγινε split στη ρίζα, επιστρέφουμε το leaf όπου έγινε η εισαγωγή
    if (!root_split.happened)
        return inserted_leaf;

    //δημιουργούμε νέο index node που θα γίνει η νέα ρίζα (έγινε split στην παλιά ρίζα)
    BF_Block *root_block;
    BF_Block_Init(&root_block);
    CALL_BF(BF_AllocateBlock(file_desc, root_block));

    char *data = BF_Block_GetData(root_block);

    //νέο block ID για τη νέα ρίζα (στο τέλος του αρχείου)
    int new_root_id;
    CALL_BF(BF_GetBlockCounter(file_desc, &new_root_id));
    new_root_id--;

    //η νέα ρίζα είναι index node με ένα κλειδί και δύο παιδιά
    IndexNodeHeader *h = (IndexNodeHeader*)data;
    h->is_leaf = 0;
    h->num_keys = 1;

    int *keys = index_node_keys(data);
    int *children = index_node_children(data, meta->max_keys);

    keys[0]       = root_split.promoted_key;    //κλειδί προαγμένο από παλιά ρίζα
    children[0]   = meta->root_block;           //αριστερό παιδί: παλιά ρίζα
    children[1]   = root_split.right_block;     //δεξί παιδί: νέος κόμβος από split παλιάς ρίζας

    BF_Block_SetDirty(root_block);
    CALL_BF(BF_UnpinBlock(root_block));
    BF_Block_Destroy(&root_block);

    //ενημερώνουμε metadata B+ tree με νέα ρίζα και αυξημένο ύψος
    meta->root_block = new_root_id;
    meta->height++;

    if (write_metadata(file_desc, meta) != 0) {
        return -1;
    }

    return inserted_leaf;
}

// ---------------------------------------------------------------------------
//       1) Ξεκινάμε από τη ρίζα.
//       2) Αν είμαστε σε index node → βρίσκουμε ποιο child πρέπει να ακολουθήσουμε.
//       3) Επαναλαμβάνουμε μέχρι να φτάσουμε σε leaf node.
//       4) Στο leaf κάνουμε γραμμική αναζήτηση (τα leaf είναι μικρά).
//
//   - Αν το key βρεθεί:
//         -> κάνουμε malloc νέα Record δομή
//         -> αντιγράφουμε την εγγραφή για να την επιστρέψουμε καθαρά στον caller
//
//   - Αν δεν βρεθεί:
//         -> επιστρέφουμε -1.
// ---------------------------------------------------------------------------
int bplus_record_find(const int file_desc,
                      const BPlusMeta *meta,
                      int key,
                      Record **out_record)
{
    //αρχικοποίηση αποτελέσματος σε NULL (δεν βρέθηκε)
    *out_record = NULL;

    //ξεκινάμε από τη ρίζα
    int current = meta->root_block;

    BF_Block *block;
    BF_Block_Init(&block);

    while (1) {
        CALL_BF(BF_GetBlock(file_desc, current, block));
        char *data = BF_Block_GetData(block);

        int is_leaf = *((int*)data);

        //αν δεν είμαστε σε leaf, κατεβαίνουμε στο κατάλληλο παιδί
        if (is_leaf == 0) {
            int next_child =
                index_node_find_child(data, key, meta->max_keys);

            CALL_BF(BF_UnpinBlock(block));
            current = next_child;
            continue;
        }

        //αναζητούμε εγγραφή στο leaf node
        int pos = data_node_find_record(data, key, &meta->schema);

        if (pos >= 0) {
            Record *found = data_node_get_record(data, pos, &meta->schema);
            if (found != NULL) {
                //βρέθηκε εγγραφή με το ζητούμενο κλειδί
                if (*out_record == NULL) { *out_record = malloc(sizeof(Record)); }
                if (*out_record != NULL) {
                    //αρχικοποίηση της δομής Record με μηδενικά για αποφυγή σκουπιδιών
                    memset(*out_record, 0, sizeof(Record));
                    // αντιγραφή εγγραφής στη μνήμη του καλούντα
                    // μόνο record_size bytes
                    memcpy(*out_record, found, meta->schema.record_size);
                }
            }
        }

        CALL_BF(BF_UnpinBlock(block));
		BF_Block_Destroy(&block);
        return (*out_record == NULL) ? -1 : 0;
    }
}
