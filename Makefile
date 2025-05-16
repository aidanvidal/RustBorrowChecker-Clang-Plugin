# Paths
LLVM_PATH := PATH_TO_LLVM_BUILD_FOLDER
CLANG := $(LLVM_PATH)/bin/clang
CMAKE := cmake
BUILD_DIR := build
PLUGIN_LIB := $(BUILD_DIR)/libBorrowCheckPlugin.dylib
TEST_SRC := test.cpp
OUTPUT := test
SDKROOT := $(shell xcrun --show-sdk-path)

# Build the plugin
plugin:
	$(CMAKE) -B $(BUILD_DIR) -DCMAKE_PREFIX_PATH=$(LLVM_PATH)/lib/cmake
	$(CMAKE) --build $(BUILD_DIR)

# Run plugin (no compilation output)
runplugin: $(PLUGIN_LIB)
	$(CLANG) -std=c++17 --stdlib=libc++ \
		-isystem $(SDKROOT)/usr/include/c++/v1 \
		-isysroot $(SDKROOT) \
		-Xclang -load -Xclang $(PLUGIN_LIB) \
		-Xclang -plugin -Xclang borrow-check \
		$(TEST_SRC)

# Build test.cpp to an executable (without plugin)
test:
	clang++ -std=c++17 $(TEST_SRC) -o $(OUTPUT)

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT)