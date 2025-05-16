#include "ownership.h"
#include <iostream>
#include <vector>
Unique<int> globalData(new int(0));

void foo()
{
    // Example function to demonstrate usage
    Unique<int> data(new int(42));
    Borrowed<int> b = data.borrow();
    BorrowedMut<int> bm = data.borrow_mut(); // This will throw an error since b is still alive
    Borrowed<int> b2 = globalData.borrow();
}

int main()
{
    Unique<int> data(new int(42));
    BorrowedMut<int> bm = data.borrow_mut();
    {
        Unique<int> data(new int(100));
        {
            Borrowed<int> b2 = data.borrow();
        }
        BorrowedMut<int> bm2 = data.borrow_mut();
        BorrowedMut<int> bm3 = globalData.borrow_mut();
    }
    Borrowed<int> b = globalData.borrow();

    // The case below the compiler does not catch
    // the runtime error will be thrown, but just be aware that
    // the compiler will not catch this
    std::vector<Borrowed<int>> borrowed;
    {
        Unique<int> test(new int(100));
        for (int i = 0; i < 10; ++i)
        {
            borrowed.push_back(test.borrow());
        }
    } // test goes out of scope here, but the borrowed references are still alive in the vector
    // This will throw a runtime error

    return 0;
}