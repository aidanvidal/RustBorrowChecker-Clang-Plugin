# C++ Ownership Model and Borrow Checker Plugin

## Description
This project implements a simple ownership model in C++, inspired by [Rust's borrow checker](https://doc.rust-lang.org/1.8.0/book/references-and-borrowing.html). It provides classes for ownership and borrowing of resources, ensuring safe memory management by enforcing strict ownership semantics. The project also includes a Clang plugin that performs static analysis to catch violation of these ownership and borrowing rules at compile time. 

The ownership.h implements the custom classes using a [RAII](https://en.cppreference.com/w/cpp/language/raii) technique. The BorrowCheckPlugin.cpp implements the Clang plugin which will check for borrow checking
ensuring that there are no overlapping mutable borrows and no mutable borrows during immutable borrows.

## Installation and Setup
### Prerequisites
- LLVM/Clang installed on your system. The plugin is designed to work with Clang, so make sure to have the development tools and libraries installed.
- CMake for building the project.

### Steps to Install
1. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/ownership-model.git
   cd ownership-model
   ```

2. Configure and build the plugin:
   Make sure to adjust the `LLVM_PATH` in the `Makefile` to point to your LLVM build folder.
   ```bash
   make plugin
   ```

## Usage
### Compiling and Running the Plugin
You can run the plugin on your C++ source files to analyze the borrow semantics:

```bash
make runplugin
```

### Running the Test
To compile the `test.cpp` file into an executable without the plugin:
```bash
make test
```
Then run the executable:
```bash
./test
```

### Example Code
The provided `test.cpp` file demonstrates how to use the `Unique`, `Borrowed`, and `BorrowedMut` classes. Below is a brief overview of their usage:
```cpp
Unique<int> data(new int(42));
Borrowed<int> b = data.borrow();  // Borrowing immutably
BorrowedMut<int> bm = data.borrow_mut(); // This will throw an error if 'b' is still alive
```

## Requirements
- C++17 compatible compiler (Clang or GCC)
- The project needs Clang libraries for the plugin to work correctly.

## Additional Information
The project defines a custom exception handling mechanism for borrow checker violations through the `BorrowError` class. This allows for graceful error reporting during runtime. 

For advanced usage, you can extend the `BorrowCheckerVisitor` to add more features to the analysis provided by the plugin. 

This is my first time making a Clang plugin so there was a lot to learn, but the [documentation](https://clang.llvm.org/doxygen/namespaceclang.html) was really helpful in that.

## License
This project is licensed under the MIT License - see the LICENSE file for details.