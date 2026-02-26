bplus_main_compile:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bplus_main.c ./src/*.c -lbf -o ./build/bp_main -O2;


bplus_main_run: bplus_main_compile
	@echo " Running bf_main ..."
	rm -f *.db
	./build/bp_main

test_bplus_compile:
	$(CC) examples/test_bplus_basic.c src/*.c -Iinclude -Llib -lbf -o build/test_bplus

test_bplus:
	LD_LIBRARY_PATH=./lib ./build/test_bplus
############################################################
# EXTRA SEPARATE UNIT TEST TARGETS (added as requested)
############################################################

test_leaf_split:
	$(CC) $(CFLAGS) tests/test_leaf_split.c src/*.c -Iinclude -Llib -lbf -o build/test_leaf_split
	LD_LIBRARY_PATH=./lib ./build/test_leaf_split

test_big_insert:
	$(CC) $(CFLAGS) tests/test_big_insert.c src/*.c -Iinclude -Llib -lbf -o build/test_big_insert
	LD_LIBRARY_PATH=./lib ./build/test_big_insert
test_insert_find:
	@echo "Compiling test_insert_find..."
	$(CC) $(CFLAGS) tests/test_insert_find.c src/*.c -Iinclude -Llib -lbf -o build/test_insert_find
	@echo "Running test_insert_find..."
	LD_LIBRARY_PATH=./lib ./build/test_insert_find

test_split_duplicate:
	@echo "Compiling test_split_duplicate..."
	$(CC) $(CFLAGS) tests/test_split_duplicate.c src/*.c -Iinclude -Llib -lbf -o build/test_split_duplicate
	@echo "Running test_split_duplicate..."
	LD_LIBRARY_PATH=./lib ./build/test_split_duplicate

test_multilevel_splits:
	@echo "Compiling test_multilevel_splits..."
	$(CC) $(CFLAGS) tests/test_multilevel_splits.c src/*.c -Iinclude -Llib -lbf -o build/test_multilevel_splits
	@echo "Running test_multilevel_splits..."
	LD_LIBRARY_PATH=./lib ./build/test_multilevel_splits

# Run all functional tests (insert/find, splits, duplicates, multilevel)
test_functional: test_insert_find test_split_duplicate test_multilevel_splits
	@echo "Functional tests completed"
