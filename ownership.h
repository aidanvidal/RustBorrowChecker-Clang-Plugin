// ownership.h
// This file defines a simple ownership model in C++ that mimics Rust's borrow checker.

#include <stdexcept>
#include <string>

// Custom error class for borrow checker violations
class BorrowError : public std::runtime_error
{
public:
    enum class ErrorCode
    {
        DestroyWithActiveBorrows,
        MoveWithActiveBorrows,
        MoveIntoWithActiveBorrows,
        MoveFromWithActiveBorrows,
        MutableBorrowOfImmutablyBorrowed,
        MutableBorrowOfMutablyBorrowed,
        ImmutableBorrowOfMutablyBorrowed,
        AccessWhileBorrowed,
        AccessWhileMutablyBorrowed,
        ReleaseNonExistentImmutableBorrow,
        ReleaseNonExistentMutableBorrow
    };

private:
    ErrorCode code_;

public:
    BorrowError(const std::string &message, ErrorCode code)
        : std::runtime_error(message), code_(code) {}

    ErrorCode code() const { return code_; }
};

// Forward declarations
template <typename T>
class Borrowed;
template <typename T>
class BorrowedMut;
template <typename T>
class Unique;

// Unique - Enforce ownership semantics (no copies, moves only).
template <typename T>
class Unique
{
    T *data;
    mutable int immutable_borrows = 0;     // Track immutable borrow count
    mutable bool mutable_borrowed = false; // Track mutable borrow state

public:
    Unique(T *ptr) : data(ptr) {}
    ~Unique() noexcept(false)
    {
        // Ensure there are no active borrows when destroying
        if (immutable_borrows > 0 || mutable_borrowed)
        {
            throw BorrowError("Cannot destroy Unique while it is borrowed", BorrowError::ErrorCode::DestroyWithActiveBorrows);
        }
        delete data;
    }

    // Disallow copying (enforce move semantics)
    // These operators are deleted to prevent copying
    Unique(const Unique &) = delete;
    Unique &operator=(const Unique &) = delete;

    // Allow moving
    // This constructor transfers ownership of the resource from 'other' to 'this'.
    Unique(Unique &&other) noexcept(false) : data(other.data),
                                             immutable_borrows(other.immutable_borrows),
                                             mutable_borrowed(other.mutable_borrowed)
    {
        // Check if the object being moved from has active borrows
        if (immutable_borrows > 0 || mutable_borrowed)
        {
            throw BorrowError("Cannot move Unique while it is borrowed", BorrowError::ErrorCode::MoveWithActiveBorrows);
        }
        other.data = nullptr;
        other.immutable_borrows = 0;
        other.mutable_borrowed = false;
    }

    // This operator transfers ownership of the resource from 'other' to 'this'.
    Unique &operator=(Unique &&other) noexcept(false)
    {
        if (this != &other)
        {
            // Check if the destination object has active borrows
            if (immutable_borrows > 0 || mutable_borrowed)
            {
                throw BorrowError("Cannot move into Unique while it is borrowed", BorrowError::ErrorCode::MoveIntoWithActiveBorrows);
            }

            // Check if the source object has active borrows
            if (other.immutable_borrows > 0 || other.mutable_borrowed)
            {
                throw BorrowError("Cannot move from Unique while it is borrowed", BorrowError::ErrorCode::MoveFromWithActiveBorrows);
            }

            delete data;          // Clean up current resource
            data = other.data;    // Transfer ownership
            other.data = nullptr; // Nullify the moved-from object

            // Transfer borrow state (should be zero at this point)
            immutable_borrows = other.immutable_borrows;
            mutable_borrowed = other.mutable_borrowed;
            other.immutable_borrows = 0;
            other.mutable_borrowed = false;
        }
        return *this;
    }

    // Borrow tracking methods
    void acquireImmutableBorrow() const
    {
        if (mutable_borrowed)
        {
            throw BorrowError("Cannot immutably borrow: already mutably borrowed", BorrowError::ErrorCode::MutableBorrowOfMutablyBorrowed);
        }
        immutable_borrows++;
    }

    void releaseBorrowImmutable() const
    {
        if (immutable_borrows <= 0)
        {
            throw BorrowError("Attempting to release non-existent immutable borrow", BorrowError::ErrorCode::ReleaseNonExistentImmutableBorrow);
        }
        immutable_borrows--;
    }

    void acquireMutableBorrow() const
    {
        if (immutable_borrows > 0 || mutable_borrowed)
        {
            throw BorrowError("Cannot mutably borrow: already borrowed", BorrowError::ErrorCode::MutableBorrowOfImmutablyBorrowed);
        }
        mutable_borrowed = true;
    }

    void releaseMutableBorrow() const
    {
        if (!mutable_borrowed)
        {
            throw BorrowError("Attempting to release non-existent mutable borrow", BorrowError::ErrorCode::ReleaseNonExistentMutableBorrow);
        }
        mutable_borrowed = false;
    }

    // Borrow methods
    Borrowed<T> borrow() const
    {
        return Borrowed<T>(this, data);
    }

    BorrowedMut<T> borrow_mut()
    {
        return BorrowedMut<T>(this, data);
    }

    // Accessors
    T *operator->()
    {
        if (immutable_borrows > 0 || mutable_borrowed)
        {
            throw BorrowError("Cannot access directly while borrowed", BorrowError::ErrorCode::AccessWhileBorrowed);
        }
        return data;
    }

    const T *operator->() const
    {
        if (mutable_borrowed)
        {
            throw BorrowError("Cannot access directly while mutably borrowed", BorrowError::ErrorCode::AccessWhileMutablyBorrowed);
        }
        return data;
    }

    T &operator*()
    {
        if (immutable_borrows > 0 || mutable_borrowed)
        {
            throw BorrowError("Cannot access directly while borrowed", BorrowError::ErrorCode::AccessWhileBorrowed);
        }
        return *data;
    }

    const T &operator*() const
    {
        if (mutable_borrowed)
        {
            throw BorrowError("Cannot access directly while mutably borrowed", BorrowError::ErrorCode::AccessWhileMutablyBorrowed);
        }
        return *data;
    }

    T *get()
    {
        if (immutable_borrows > 0 || mutable_borrowed)
        {
            throw BorrowError("Cannot access directly while borrowed", BorrowError::ErrorCode::AccessWhileBorrowed);
        }
        return data;
    }

    const T *get() const
    {
        if (mutable_borrowed)
        {
            throw BorrowError("Cannot access directly while mutably borrowed", BorrowError::ErrorCode::AccessWhileMutablyBorrowed);
        }
        return data;
    }

    explicit operator bool() const { return data != nullptr; } // Check if the pointer is valid
};

// Borrowed - Represent immutable borrows with restricted lifetimes
template <typename T>
class Borrowed
{
    const Unique<T> *owner_;
    const T *data;

public:
    explicit Borrowed(const Unique<T> *owner, const T *ptr) : owner_(owner), data(ptr)
    {
        owner_->acquireImmutableBorrow();
    }

    // Copy constructor
    Borrowed(const Borrowed &other) : owner_(other.owner_), data(other.data)
    {
        owner_->acquireImmutableBorrow();
    }

    // Copy assignment
    Borrowed &operator=(const Borrowed &other)
    {
        if (this != &other)
        {
            owner_->releaseBorrowImmutable();
            owner_ = other.owner_;
            data = other.data;
            owner_->acquireImmutableBorrow();
        }
        return *this;
    }

    // Destructor
    ~Borrowed()
    {
        owner_->releaseBorrowImmutable();
    }

    const T *operator->() const { return data; }
    const T &operator*() const { return *data; }
    const T *get() const { return data; }
    explicit operator bool() const { return data != nullptr; }
};

// BorrowedMut - Represent mutable borrows with restricted lifetimes
template <typename T>
class BorrowedMut
{
    const Unique<T> *owner_;
    T *data;

public:
    explicit BorrowedMut(const Unique<T> *owner, T *ptr) : owner_(owner), data(ptr)
    {
        owner_->acquireMutableBorrow();
    }

    // Disallow copying (enforce move semantics)
    BorrowedMut(const BorrowedMut &) = delete;
    BorrowedMut &operator=(const BorrowedMut &) = delete;

    // Move constructor
    BorrowedMut(BorrowedMut &&other) noexcept : owner_(other.owner_), data(other.data)
    {
        other.owner_ = nullptr;
        other.data = nullptr;
    }

    // Move assignment
    BorrowedMut &operator=(BorrowedMut &&other) noexcept
    {
        if (this != &other)
        {
            if (owner_)
                owner_->releaseMutableBorrow();
            owner_ = other.owner_;
            data = other.data;
            other.owner_ = nullptr;
            other.data = nullptr;
        }
        return *this;
    }

    // Destructor
    ~BorrowedMut()
    {
        if (owner_)
            owner_->releaseMutableBorrow();
    }

    T *operator->() { return data; }
    const T *operator->() const { return data; }
    T &operator*() { return *data; }
    const T &operator*() const { return *data; }
    T *get() { return data; }
    const T *get() const { return data; }
    explicit operator bool() const { return data != nullptr; }
};