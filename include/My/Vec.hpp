#ifndef MY_VECTOR_INCLUDED
#define MY_VECTOR_INCLUDED

#include <assert.h>
#include "MyInternals.hpp"
#include "Array.hpp"
#include <initializer_list>
#include "std.hpp"
#include "../constants.hpp"
#include "Allocation.hpp"
#include "llvm/ArrayRef.h"

MY_CLASS_START

namespace Vector {

namespace Generic {
    struct Vec {
        void* data;
        int size;
        int capacity;
    };

    struct GenericVec {
        void* data;
        int size;
        int capacity;
        int typeSize;

        GenericVec() = default;

        GenericVec(int typeSize, int capacity) : size(0), capacity(capacity), typeSize(typeSize) {
            // TODO: kdajflsdj 
        }

        template<typename T>
        GenericVec New() {
            GenericVec vec;
            vec.data = nullptr;
            vec.size = 0;
            vec.capacity = 0;
            vec.typeSize = sizeof(T);
            return vec;
        }

        template<typename T>
        T* get(int index) {
            return &((T*)data)[index];
        }

        void* get(int index) {
            if (index > 0 && index < size)
                return (char*)data + index * typeSize;
            return nullptr;
        }
    };

    bool vec_push(Vec* vec, int typeSize, const void* elements, int elementCount=1);
}

/* A simple vector implementation for POD ONLY types!
 * Constructors and destructors will not be called!
 * Should be faster and smaller than std::vector most of the time, 
 * and result in faster compile times
 */ 
template<typename T, class AllocatorT = DefaultAllocator>
struct Vec {
    static_assert(std::is_trivially_move_assignable<T>::value && std::is_trivially_move_constructible<T>::value, "vec doesn't support complex types");
    using Type = T;

    T* data;
    int size;
    int capacity;

    using Allocator = AllocatorT;
    static Allocator allocator;

private: 
    using Self = Vec<T, AllocatorT>;
    using ValueParamT = FastestParamType<T>;
public:

    // does not initialize data
    Vec() {
#ifdef DEBUG
        data = (T*)(1); // pointer that can't be null checked but should still crash on dereference
        size = 0x7ff8dead; // sentinel value that should mess everything up when used
        capacity = 0x7ff8dead; // sentinel value that should mess everything up when used
#endif
    }

    Vec(int startCapacity) : size(0), capacity(startCapacity) {
        if (startCapacity > 0) {
            data = allocator.template Alloc<T>(startCapacity);
        } else {
            data = nullptr;
        }
    }

    inline static Self WithCapacity(int capacity) {
        return Self(capacity);
    }

    Vec(T* _data, int _size, int _capacity)
    : data(_data), size(_size), capacity(_capacity) {}

    static Self Empty() {
        return Self{nullptr, 0, 0};
    }

    /* Create a vector with the given size and copy the given data to the vector
     * 
     */
    Vec(const T* _data, int _size) {
        data = allocator.template Alloc<T>(_size);
        memcpy(data, _data, _size * sizeof(T));
        size = _size;
        capacity = _size;
    }

    static Self Filled(int size, ValueParamT value) {
        assert(size >= 0 && "cannot have negative size");
        Self self;
        self.data = allocator.template Alloc<T>(size);
        for (int i = 0; i < size; i++) {
            memcpy(&self[i], &value, sizeof(T));
        }
        self.size = size;
        self.capacity = size;
        return self;
    }

    inline T* get(int index) const {
        if (data && index < size && index > -1) {
            return &data[index];
        }
        return nullptr;
    }

    inline T& operator[](int index) const {
#ifdef BOUNDS_CHECKS
        assert(index < size    && "vector index out of bounds");
        assert(index > -1      && "vector index out of bounds");
#endif
        assert(data != nullptr && "access of null vector");
        return data[index];
    }

    void push(ValueParamT val) {
        /*
        reserve(size+1)
        memcpy(&data[size], &val, sizeof(T));
        */
        //return vec_push((Generic::Vec*)this, sizeof(T), &val);
        if (size+1 > capacity) {
            int newCapacity = capacity*2 > size+1 ? capacity*2 : size+1;
            data = allocator.template Realloc<T>(data, newCapacity);
            capacity = newCapacity;
        }
        memcpy(data + size, &val, sizeof(T));
        ++size;
    }

    void pushFront(const T* values, int count) {
        assert(count > 0 && "can't push 0 or less values!");
        if (size + count > capacity) {
            // reallocate. Fortunately this makes the algorithm a lot simpler. just copy the new values in, then the old values
            int newCapacity = MAX(size + count, capacity*2);
            T* newData = allocator.template Alloc<T>((size_t)newCapacity);
            memcpy(&newData[0], values, (size_t)count * sizeof(T));
            memcpy(&newData[count], data, (size_t)size * sizeof(T));
            allocator.Free(data);
            data = newData;
            size += count;
            capacity = newCapacity;
        } else {
            // no allocation version, a bit more complicated
            // move all current elements forward at 'count' sized batches
            int elementsMoved = 0;
            while (elementsMoved < size) {
                int elementsLeftToMove = size - elementsMoved;
                int batchSize = elementsLeftToMove < count ? elementsLeftToMove : count;
                memcpy(&data[elementsMoved], &data[size - elementsMoved - 1], (size_t)batchSize * sizeof(T));
                elementsMoved += batchSize;
            }

            // copy new elements into now unused space at beginning
            memcpy(&data[0], values, (size_t)count * sizeof(T));
            size += count;
        }
    }

    void pushFront(ValueParamT val) {
        pushFront(&val, 1);
    }

    void pop() {
        assert(size > 0 && "can't pop element of empty vector");
        size--;
    }

    T popBack() {
        assert(size > 0 && "can't pop back element of empty vector");
        return data[--size];
    }

    bool empty() const {
        return size == 0;
    }

    // reserve atleast capacity
    // \returns the new capacity of the vec
    int reserve(int capacity) {
        assert(capacity >= 0 && "cannot have negative capacity");
        // parameter capacity shadows this->capacity, so have to explicitly use 'this'
        if (this->capacity < capacity) {
            reallocate(this->capacity*2 > capacity ? this->capacity*2 : capacity);
        }
        return this->capacity;
    }
 
    // resize the vector
    void resize(int size) {
        assert(size >= 0 && "cannot have negative size");
        reserve(size);
        this->size = size;
    }

    void resize(int size, ValueParamT value) {
        assert(size >= 0 && "cannot have negative size");
        reserve(size);
        for (int i = this->size; i < size; i++) {
            memcpy(&data[i], &value, sizeof(T));
        }
        this->size = size;
    }

    /* Resize the vector to the given new capacity. 
     * If the current size of the vector is greater than the new capacity, the size will reduced to the new capacity,
     * losing any elements past index 'newCapacity' - 1.
     * @return The capacity of the vector after resizing. If an error hasn't occured, this will be equal to 'newSize'.
     * When the new capacity couldn't be allocated, -1 will be returned
     */
    void reallocate(int newCapacity) {
        data = (T*)allocator.Realloc(data, newCapacity);
        size = (size < newCapacity) ? size : newCapacity;
        capacity = newCapacity;
    }

    void clear() {
        allocator.Free(data);
        data = nullptr;
        size = 0;
        capacity = 0;
    }

    void destroy() {
        allocator.Free(data);
    }

    T& back() const {
       assert(size > 0 && "can't get element of empty vector"); 
       return data[size-1];
    }

    T& front() const {
       assert(size > 0 && "can't get element of empty vector"); 
       return data[0];
    }

    void remove(int index) {
        assert(index < size && "vector index out of bounds");
        assert(index > -1    && "vector index out of bounds");

        for (int i = index; i < size-1; i++) {
            memcpy(&data[i], &data[i+1], sizeof(T));
        }

        size--;
    }

    bool push(const T* elements, int count) {
        /*
        if (reserveAtleast(size + count)) {
            memcpy(&data[size], elements, count * sizeof(T));
            size += count;
            return true;
        }
        return false;
        */
        return Generic::vec_push((Generic::Vec*)this, sizeof(T), elements, count);
    }

    bool push(ArrayRef<T> array) {
        return Generic::vec_push((Generic::Vec*)this, sizeof(T), array.data(), array.size());
    }

    /* Reserve space for \p size elements at the back and increase the vector's size by that amount, leaving the elements in those spaces unitialized.
     * Like a push but without initialization.
     * \return The address of the elements required.
     */
    T* require(int size) {
        reserve(this->size + size);
        this->size += size;
        return &data[this->size - size];
    }

    void insertAt(int index, const T* values, int count) {
        T* at = &data[index];
        if (size + count > capacity) {
            // reallocate. Fortunately this makes the algorithm a lot simpler. just copy the new values in, then the old values
            int newCapacity = MAX(size + count, capacity*2);
            T* newData = (T*)allocator.Alloc(newCapacity * sizeof(T));
            // copy old data before index to range 0 - index
            memcpy(&newData[0], &data[0], index * sizeof(T));
            // copy values to range index - index + count
            memcpy(&newData[index], values, count * sizeof(T));
            // copy old data after index to range index + count -  size + count
            memcpy(&newData[index + count], &data[index], (size - index) * sizeof(T));

            allocator.Free(data);
            data = newData;
            size += count;
            capacity = newCapacity;
        } else {
            // no allocation version, a bit more complicated
            // move all current elements forward at 'count' sized batches
            int elementsMoved = 0;
            while (elementsMoved < size) {
                int elementsLeftToMove = size - elementsMoved;
                int batchSize = elementsLeftToMove < count ? elementsLeftToMove : count;
                memcpy(&at[elementsMoved], &at[size - elementsMoved - 1], batchSize * sizeof(T));
                elementsMoved += batchSize;
            }

            // copy new elements into now unused space at beginning
            memcpy(&at[0], values, count * sizeof(T));
        }
    }

    template<typename T2>
    Vec<T2> cast() const {
        return Vec<T2>(data, size, capacity);
    }

    using iterator = T*; // no iterator bs, just pointer

    inline T* begin() const { return data; }
    inline T* end() const { return data + size; }
};

template<typename T, class AllocatorT>
typename Vec<T, AllocatorT>::Allocator Vec<T, AllocatorT>::allocator = Vec<T, AllocatorT>::Allocator();

static_assert(sizeof(Generic::Vec) == sizeof(Vec<int>), "GenericVec must have same binary layout as normal vec");

template<typename T, int N = 0>
struct SmallVector {
    T* data;
    int size;
    int capacity;
    char buffer[N * sizeof(T)];

    T& operator[](int index) {
        return data[index];
    }
};

/*
template<typename T1, typename T2, size_t BufferSize>
struct SmallVectorPair {
    T1* first;
    T2* second;

    int32_t size;
    int32_t capacity;

    char buffer1[BufferSize * sizeof(T1)];
    char buffer1[BufferSize * sizeof(T2)];

    SmallVectorPair() {}

    operator[]() {

    }
};
*/
/*
thing dont work! prolly not worth doing myself ever. better options!
template<typename... Ts>
struct VecTuple {
    using Size_T = int;
    using ValueTuple = std::tuple<Ts...>;
    constexpr static size_t TypeCount = sizeof...(Ts);

    using PointerTuple = std::tuple<Ts* ...>;
    PointerTuple data;
    Size_T size;
    Size_T capacity;

    template<typename T>
    void reallocateDataTupleMember(Size_T capacity) {
        std::get<T>(data) = (T*)MY_realloc(std::get<T>(data), capacity * sizeof(T));
    }

    void reallocateDataTuple(Size_T newCapacity) {
        FOR_EACH_VAR_TYPE(reallocateDataTupleMember<Ts>(newCapacity));
    }

    void reallocate(Size_T newCapacity) {
        reallocateDataTuple(newCapacity);
        // do other stuff TODO:
        size = MIN(newCapacity, size);
        capacity = newCapacity;
    }

    VecTuple(Size_T startCapacity) : data(PointerTuple()), size(0), capacity(startCapacity) {
        reallocateDataTuple(startCapacity);
    }

    #define WITHIN(lo, v, hi) (lo < v && v < hi)

    template<typename T>
    void setElements(Size_T index, Size_T count, T* list) {
        memcpy(std::get<T>(data)[index], list, count * sizeof(T));
    }

    template<typename T>
    T& get(Size_T i) {
        assert(0 <= i && i < size && "vec tuple index out of bounds!");
        return std::get<T>(data)[i];
    }

    template<typename T>
    T* getPointer() {
        return std::get<T>(data);
    }

    void push(FastestParamType<Ts> ...vals) {
        if (size+1 > capacity) {
            reallocate(MAX(size+1, capacity*2));
        }
        FOR_EACH_VAR_TYPE(setElements(size, 1, &vals));
        size++;
    }

    void push(int count, const Ts* ...lists) {
        if (size + count > capacity) {
            reallocate(MAX(size + count, capacity*2));
        }
        FOR_EACH_VAR_TYPE(setElements(size, count, lists));
        size += count;
    }
};
*/
}

using Vector::Vec;
using Vector::Generic::GenericVec;

MY_CLASS_END

#endif