#include <type_traits>
#include <iterator>


// 
// A custom vector implementation focusing on object oriented concepts.
//
//
// This implementation uses separate allocation and construction which should
// be preferred in an efficient implementation that can hold generic values.
//
// Using simply new[] and delete[] for the underlying raw data will implicitly
// allocate + construct and destroy + deallocate, respectively. This is not 
// desired for a fundamentally dynamic data structure since 
//  - we do not want to default construct a potentially large set of elements  
//    if we call reserve() which would be expensive (and not possible if T does
//    not have a default constructor). We only want to construct objects if
//    an element is added.
//  - we would be forced to use delete[] if we created the data with new[],
//    which would result in expensive reallocations if we would pop an element.
//    Single element destruction would in this case lead to double destruction.
//  - adding elements to a vector would mean assigning a new value (i.e.
//    overwriting) to the default constructed value present in that slot.
//    We would rather directly construct the element inside the slot. 
//  - ...
//
// The following line "SomeObject* so = new SomeObject();" effectively consists
// of two operations:
//  1. allocating raw bytes through "::operator new(sizeof(SomeObject))"
//     (this is basically the C++ version of malloc() with the difference that
//     malloc() returns NULL and ::operator new() throws a std::bad_alloc if
//     memory allocation fails)
//  2. invoking the constructor of SomeObject inside those bytes
//
// The following line "delete so;" effectively consists of two operations:
//  1. invoking the destructor of SomeObject "so->~SomeObject()"
//  2. deallocating raw bytes through "::operator delete(so)"
//     (this is basically the C++ version of free())
// 
// So in order to implement separate allocation + construction and destroy + 
// destruction we follow the same pattern for manual control. 
//
// 
//
// All special member functions are implemented because we manage our own raw
// resource, i.e. the underlying data array (rule of five)
// 
template <typename T>
class CustomVector {
public:

    // Default constructor for empty vector
    // cannot throw since no resources are acquired
    CustomVector() noexcept = default;

    // Constructor for size elements default constructed
    explicit CustomVector(std::size_t size) {
        allocate_and_default_construct(size);
    }

    // Constructor for size elements initialized to given value
    CustomVector(std::size_t size, const T& value) {
        allocate_and_fill(size, value);
    }

    // Constructor for ranges of iterator pairs to deep copy all elements
    // from the range.
    // uses C++20 concepts to directly constraint the InputIt type to an input
    // iterator which prevents matching to non input iterator types.
    // pre C++17 must use std::enable_if
    template <std::input_iterator InputIt>
    CustomVector(InputIt first, InputIt last) {
        allocate_and_copy(first, last);
    }

    // Constructor for given initializer list
    // enables initializing with {} list of elements
    CustomVector(std::initializer_list<T> values)
        : CustomVector(values.begin(), values.end()) {}

    // Copy constructor to define pass-by-value behavior
    CustomVector(const CustomVector& other)
        : CustomVector(other.begin(), other.end()) {}

    // Move constructor
    // in general must be noexcept, moving should never throw
    // because no throwable operations are performed
    CustomVector(CustomVector&& other) noexcept:
        data_{ std::exchange(other.data_, nullptr) },
        size_{ std::exchange(other.size_, 0) },
        capacity_{ std::exchange(other.capacity_, 0) }
    {}

    // Copy assignment operator
    auto operator=(const CustomVector& other) -> CustomVector& {
        if (this == &other) return; // self assignment check

        // make a full deep copy of other by using the copy constructor which
        // could throw, after successful copying use noexcept swap
        auto copy = CustomVector(other);
        std::swap(data_, copy.data_);
        std::swap(size_, copy.size_);
        std::swap(capacity_, copy.capacity_);

        // destructor of copy will automatically clean up
        return *this;
    }

    // Move assignment operator
    auto operator=(CustomVector&& other) noexcept -> CustomVector& {
        if (this == &other) return; // self assignment check

        // release resources currently managed by this vector
        std::destroy(data_, get_iterator_at_index(size_));
        CustomVector::deallocate(data_);

        // swap, i.e. steal resources from other
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
        capacity_ = std::exchange(other.capacity_, 0);

        return *this;
    }

    // Destructor
    ~CustomVector() noexcept {
        // destruct all elements
        std::destroy(data_, get_iterator_at_index(size_));

        // deallocate memory
        CustomVector::deallocate(data_);
    }

public:

    // Allocate memory for given capacity, transfer elements to new memory
    // location, does not change size of vector
    auto reserve(std::size_t capacity) -> void {
        if (capacity <= capacity_) return;

        reallocate(capacity);
    }

    auto resize(std::size_t size) -> void {
        // Todo
    }

    // reduce capacity to match size
    auto shrink_to_fit() -> void {
        if (size_ == capacity_) return;

        if (size_ == 0) {
            CustomVector::deallocate(data_);
            data_ = nullptr;
            capacity_ = 0;
        }
        
        reallocate(size_);
    }

    auto push_back(const T& value) -> void {
        emplace_back(value);
    }

    auto push_back(T&& value) -> void {
        emplace_back(std::move(value));
    }

    // emplace_back can construct any type directly avoiding temporaries
    template<typname... Args>
    auto emplace_back(Args&&... args) -> T& {
        
        // capacity reached, need to reallocate
        if (size_ == capacity_) reserve(get_new_capacity());
        
        // simply construct the element directly at the end of the vector
        std::construct_at(
            get_iterator_at_index(size_),
            std::forward<Args>(args)...
        );

        ++size_;
        return back();
    }

    auto clear() noexcept -> void {
        std::destroy(data_, get_iterator_at_index(size_));
        size_ = 0;
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        return size_ == 0;
    }

public: // Element access

    // does not throw, no bounds check, use for known valid indices
    [[nodiscard]] auto operator[](std::size_t index) noexcept -> T& {
        return data_[index];
    }

    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> T& {
        return data_[index];
    } 

    // throws if out of range
    // NOTE: should be preferred if the index comes from user input or
    //       external data
    [[nodiscard]] auto at(std::size_t index) -> T& {
        if (index >= size_) {
            throw std::out_of_range("[CustomVector::at()] Index out of range!");
        }

        return data_[index];
    }

    [[nodiscard]] auto at(std::size_t index) const -> T& {
        if (index >= size_) {
            throw std::out_of_range("[CustomVector::at()] Index out of range!");
        }

        return data_[index];
    }

    [[nodiscard]] auto front() noexcept -> T& {
        return data_[0]; // assumes non empty vector
    }

    [[nodiscard]] auto front() const noexcept -> const T& {
        return data_[0]; // assumes non empty vector
    }

    [[nodiscard]] auto back() noexcept -> T* {
        return data_[size_ - 1]; // assumes non empty vector
    }

    [[nodiscard]] auto back() const noexcept -> const T* {
        return data_[size_ - 1]; // assumes non empty vector
    }

public: // Member access

    [[nodiscard]] auto data() noexcept -> T* {
        return data_;
    }

    [[nodiscard]] auto data() const noexcept -> const T* {
        return data_;
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return size_;
    }

    [[nodiscard]] auto capacity() const noexcept -> std::size_t {
        return capacity_;
    }

public: // Iterators

    [[nodiscard]] auto begin() noexcept -> T* {
        return data_;
    }

    [[nodiscard]] auto end() noexcept -> T* {
        return get_iterator_at_index(size_);
    }

    [[nodiscard]] auto cbegin() const noexcept -> const T* {
        return data_;
    }

    [[nodiscard]] auto cend() const noexcept -> const T* {
        return get_iterator_at_index(size_);
    }

private:

    // Allocate raw bytes
    [[nodiscard]] static auto allocate(std::size_t n) -> T* {
        if (n == 0) return nullptr;
        
        // similar to (T*)malloc(n * sizeof(T))
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    // Deallocate raw bytes
    static auto deallocate(T* p) noexcept -> void {
        // similar to free(p)
        ::operator delete(p);
    }

    // Reallocate memory based on given capacity and copy/move elements to new
    // memory
    auto reallocate(std::size_t capacity) -> void {
        T* new_data = CustomVector::allocate(capacity);

        try {

            // This moves or falls back to copying all elements in the range 
            // into the allocated memory using std::uninitialized_move
            // or std::uninitialized_copy
            uninitialized_move_or_copy(
                data_,
                get_iterator_at_index(size_),
                new_data
            );

        } catch(...) {

            // If uninitialized_move_or_copy() fails it cleans up (in the case 
            // of copying it destructs all previously constructed elements) to
            // ensure strong exception safety. So we only need to deallocate
            // the previously allocated memory.
            CustomVector::deallocate(new_data);

            throw;
        }

        // now we can clean up old memory
        std::destroy(data_, get_iterator_at_index(size_));
        CustomVector::deallocate(data_);

        // Set attributes if allocation and initialization were successful
        data_ = new_data;
        capacity_ = capacity; 
    }

    [[nodiscard]] auto get_new_capacity() const noexcept -> std::size_t {
        return capacity_ == 0 ? 1 : capacity_ * 2;
    }

private:

    auto allocate_and_default_construct(std::size_t size) -> void {
        if (size == 0) return;

        T* new_data = CustomVector::allocate(size);
        
        try {

            // This basically default constructs all elements directly
            // inside the allocated memory using std::construct_at() or ::new
            std::uninitialized_default_construct(new_data, size);

        } catch(...) {

            // If std::uninitialized_default_construct() fails to construct
            // all elements, it will call std::destroy() to destruct the range 
            // of all previously constructed elements to ensure strong exception
            // safety. So we only need to deallocate the previously allocated
            // memory.
            CustomVector::deallocate(new_data);

            throw;
        }

        // Set attributes if allocation and initialization were successful
        data_ = new_data;
        size_ = size;
        capacity_ = size;
    }

    auto allocate_and_fill(std::size_t size, const T& value) -> void {
        if (size == 0) return;

        T* new_data = CustomVector::allocate(size);
        
        try {

            // This basically fills the allocated memory directly with copies
            // of the given value using std::construct_at() or ::new
            std::uninitialized_fill(new_data, size, value);

        } catch(...) {

            // If std::uninitialized_fill() fails to construct all elements, it
            // will invoke the destructor of each previously constructed
            // element to ensure strong exception safety. So we only need to
            // deallocate the previously allocated memory.
            CustomVector::deallocate(new_data);

            throw;
        }

        // Set attributes if allocation and initialization were successful
        data_ = new_data;
        size_ = size;
        capacity_ = size; 
    }

    template <std::input_iterator InputIt>
    auto allocate_and_copy(InputIt first, InputIt last) -> void {
        const auto size = std::static_cast<std::size_t>(
            std::distance(first, last)
        );

        if (size == 0) return;

        T* new_data = CustomVector::allocate(size);

        try {

            // This basically copy constructs all elements in the range directly
            // inside the allocated memory using std::construct_at() or ::new
            std::uninitialized_copy(first, last, new_data);

        } catch(...) {

            // If std::uninitialized_copy() fails to construct all elements, it
            // will invoke the destructor of each previously constructed
            // element to ensure strong exception safety. So we only need to
            // deallocate the previously allocated memory.
            CustomVector::deallocate(new_data);

            throw;
        }

        // Set attributes if allocation and initialization were successful
        data_ = new_data;
        size_ = size;
        capacity_ = size; 
    }

    template <std::input_iterator InputIt, std::forward_iterator ForwardIt>
    auto uninitialized_move_or_copy(
        InputIt src_first,
        InputIt src_last,
        ForwardIt dest_first
    ) -> ForwardIt {
        // get type of elements of input iterator
        using ElementType = typename std::iterator_traits<InputIt>::value_type;

        // we move all elements if element type is movable with noexcept 
        // move operations or if element type is not copyable (fallback)
        if constexpr (
            std::is_nothrow_move_constructible_v<ElementType> ||
            !std::is_copy_constructible_v<ElementType>
        ) {
            return std::uninitialized_move(src_first, src_last, dest_first);
        } else {
            return std::uninitialized_copy(src_first, src_last, dest_first);
        }
    }

private:

    [[nodiscard]] auto get_iterator_at_index(
        std::size_t index
    ) noexcept -> T* {
        return data_ == nullptr ? nullptr : data_ + index; 
    }

    [[nodiscard]] auto get_iterator_at_index(
        std::size_t index
    ) const noexcept -> const T* {
        return data_ == nullptr ? nullptr : data_ + index; 
    }

private:

    // Actual raw data array
    T* data_ = nullptr;

    // Number of elements currently in the vector
    std::size_t size_ = 0;

    // Number of elements for which memory is allocated
    std::size_t capacity_ = 0;
};
