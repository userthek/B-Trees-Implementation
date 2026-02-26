# B+ Tree: Αρχιτεκτονική & Υλοποίηση

## Δομή έργου
- include/: συμβολές (schemas, κόμβοι, metadata, BF API).
- src/: υλοποιήσεις (`bplus_file_funcs.c`, `bplus_datanode.c`, `bplus_index_node.c`, `record.c`).
- tests/, examples/: αρχεία δοκιμών/παραδειγμάτων.
- BF layer: `bf.h`/BF_* χειρίζονται blocks σταθερού μεγέθους `BF_BLOCK_SIZE`.

## Block 0: Μεταδεδομένα (struct `BPlusMeta`)
- `schema`: πλήρες `TableSchema` (ονόματα, τύποι, offsets, `record_size`, `key_index`).
- `root_block`: id ρίζας.
- `height`: ύψος (ρίζα = 1).
- `first_leaf` / `last_leaf`: άκρα για γρήγορο range scan.
- `max_records`: (BF_BLOCK_SIZE - sizeof(DataNodeHeader)) / schema->record_size.
- `max_keys`: ((BF_BLOCK_SIZE - sizeof(IndexNodeHeader)) / sizeof(int) - 1) / 2.
- Γράφεται/φορτώνεται με `write_metadata` και `bplus_open_file`.

## Leaf blocks (Data Nodes) — `bplus_datanode.*`
- Layout: `DataNodeHeader { is_leaf=1, num_records, next_leaf }` + διαδοχικά records σε αύξουσα σειρά κλειδιού.
- `data_node_insert_record`: ελέγχει χωρητικότητα/διπλότυπα, κάνει shift και εισάγει ταξινομημένα.
- `data_node_get_record`: pointer σε N-οστό record.
- `data_node_find_record`: γραμμική αναζήτηση μέσα στο leaf.
- `data_node_print`: helper debug.
- `max_records` υπολογίζεται στο create/open.

## Index blocks — `bplus_index_node.*`
- Layout: `IndexNodeHeader { is_leaf=0, num_keys }` + `keys[]` + `children[]`.
  - Πάντα `children count = num_keys + 1`.
  - Πίνακες continuous στη μνήμη: πρώτα keys, μετά children (με μέγιστο `max_keys` από metadata).
- `index_node_init`: μηδενίζει header.
- `index_node_find_child`: επιλέγει child όπου key < επόμενου key (classic B+ rule).
- `index_node_insert_key`: shift/insert ταξινομημένα (αν υπάρχει χώρος, αλλιώς -1).
- `index_node_print`: debug dump.

## Διαχείριση αρχείου & πράξεις δέντρου — `bplus_file_funcs.c`
- Μακροεντολή `CALL_BF` τυλίγει BF_* κλήσεις.
- `compute_max_records`/`compute_max_keys`: χωρητικότητα κόμβων από `BF_BLOCK_SIZE`.
- `bplus_create_file`:
  - BF_Create/ BF_Open.
  - Block 0: γράφει `BPlusMeta` (ρίζα=1, ύψος=1, first/last leaf=1, χωρητικότητες).
  - Block 1: αρχικοποιεί ρίζα ως leaf (`is_leaf=1`, `num_records=0`, `next_leaf=-1`).
- `bplus_open_file`: ανοίγει και διαβάζει block 0 σε malloc’ed `BPlusMeta`.
- `bplus_close_file`: free metadata + BF_Close.
- Εισαγωγή (`bplus_record_insert`):
  - Αναδρομή `insert_into_node`: σε leaf -> προσπάθεια insert, αν γεμάτο -> `split_leaf`.
  - `split_leaf`: μοιράζει τα μισά records σε νέο leaf, ενημερώνει `next_leaf`, `first_leaf`/`last_leaf`, γράφει metadata. Προάγει πρώτο key του δεξιού leaf προς τα πάνω.
  - Σε index: αναδρομή στον κατάλληλο child (με `index_node_find_child`). Αν ο child χωρίστηκε, επιχειρεί `index_node_insert_key`; αν γεμάτος, `split_index_node` και επιστρέφει προαγωγή.
  - Αν η ρίζα χωριστεί, φτιάχνεται νέο root index με 1 key και 2 children, αυξάνει `height`, ενημερώνεται block 0.
  - Επιστρέφει block id του leaf όπου κατέληξε το record (ή -1 σε σφάλμα/διπλότυπο).
- Αναζήτηση (`bplus_record_find`):
  - Κατάβαση indexes με `index_node_find_child`.
  - Στο leaf: `data_node_find_record`, αντιγραφή σε νεο-alloc’ed `Record` αν βρεθεί.
- Metadata updates: κάθε split leaf ενημερώνει `first_leaf`/`last_leaf` και επανεγγράφει block 0. Όλα τα blocks unpin/destroy μέσω BF API.

## Συμβάσεις/αντοχές
- Τα keys είναι int και μοναδικά (διπλότυπα οδηγούν σε -1/μήνυμα).
- Ταξινόμηση αυστηρά αύξουσα σε leaves και indexes.
- Απαραίτητο `BF_Block_SetDirty` πριν από `BF_UnpinBlock` όταν γίνεται τροποποίηση.
- `max_keys`/`max_records` πρέπει να ξαναϋπολογίζονται αν αλλάξει `BF_BLOCK_SIZE` ή schema.

## Γρήγορη χρήση
- Ορισμός schema με `schema_init`.
- Δημιουργία αρχείου: `bplus_create_file(&schema, "tree.bplus");`
- Άνοιγμα: `bplus_open_file("tree.bplus", &fd, &meta);`
- Insert: `bplus_record_insert(fd, meta, &rec);`
- Find: `bplus_record_find(fd, meta, key, &out);`
- Κλείσιμο: `bplus_close_file(fd, meta);`

### Αρχεία Ελέγχου `tests/`
- `test_create_open_close.c`: Ελέγχει τη σωστή δημιουργία, άνοιγμα και κλείσιμο του αρχείου B+, καθώς και την αρχικοποίηση των metadata.
- `test_simple_insert.c`: Ελέγχει απλές εισαγωγές εγγραφών χωρίς να προκαλείται διάσπαση (split).
- `test_insert_find.c`: Ελέγχει την ορθότητα της εισαγωγής και της αναζήτησης (find), επιβεβαιώνοντας ότι τα δεδομένα ανακτώνται σωστά.
- `test_leaf_split.c`: Ελέγχει τη διάσπαση φύλλου (leaf split) όταν γεμίσει ένα block δεδομένων.
- `test_split_duplicate.c`: Ελέγχει τη συμπεριφορά σε προσπάθεια εισαγωγής διπλότυπων κλειδιών (πρέπει να αποτυγχάνει) και τη σωστή λειτουργία του split.
- `test_multilevel_splits.c`: Ελέγχει διαδοχικές διασπάσεις που οδηγούν σε αύξηση του ύψους του δέντρου (index splits).
- `test_big_insert.c`: Stress test με μαζικές εισαγωγές για έλεγχο σταθερότητας σε μεγάλο όγκο δεδομένων.
