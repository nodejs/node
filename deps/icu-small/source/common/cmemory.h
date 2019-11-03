// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File CMEMORY.H
*
*  Contains stdlib.h/string.h memory functions
*
* @author       Bertrand A. Damiba
*
* Modification History:
*
*   Date        Name        Description
*   6/20/98     Bertrand    Created.
*  05/03/99     stephen     Changed from functions to macros.
*
******************************************************************************
*/

#ifndef CMEMORY_H
#define CMEMORY_H

#include "unicode/utypes.h"

#include <stddef.h>
#include <string.h>
#include "unicode/localpointer.h"

#if U_DEBUG && defined(UPRV_MALLOC_COUNT)
#include <stdio.h>
#endif


#define uprv_memcpy(dst, src, size) U_STANDARD_CPP_NAMESPACE memcpy(dst, src, size)
#define uprv_memmove(dst, src, size) U_STANDARD_CPP_NAMESPACE memmove(dst, src, size)

/**
 * \def UPRV_LENGTHOF
 * Convenience macro to determine the length of a fixed array at compile-time.
 * @param array A fixed length array
 * @return The length of the array, in elements
 * @internal
 */
#define UPRV_LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))
#define uprv_memset(buffer, mark, size) U_STANDARD_CPP_NAMESPACE memset(buffer, mark, size)
#define uprv_memcmp(buffer1, buffer2, size) U_STANDARD_CPP_NAMESPACE memcmp(buffer1, buffer2,size)
#define uprv_memchr(ptr, value, num) U_STANDARD_CPP_NAMESPACE memchr(ptr, value, num)

U_CAPI void * U_EXPORT2
uprv_malloc(size_t s) U_MALLOC_ATTR U_ALLOC_SIZE_ATTR(1);

U_CAPI void * U_EXPORT2
uprv_realloc(void *mem, size_t size) U_ALLOC_SIZE_ATTR(2);

U_CAPI void U_EXPORT2
uprv_free(void *mem);

U_CAPI void * U_EXPORT2
uprv_calloc(size_t num, size_t size) U_MALLOC_ATTR U_ALLOC_SIZE_ATTR2(1,2);

/**
 * Get the least significant bits of a pointer (a memory address).
 * For example, with a mask of 3, the macro gets the 2 least significant bits,
 * which will be 0 if the pointer is 32-bit (4-byte) aligned.
 *
 * uintptr_t is the most appropriate integer type to cast to.
 */
#define U_POINTER_MASK_LSB(ptr, mask) ((uintptr_t)(ptr) & (mask))

/**
 * Create & return an instance of "type" in statically allocated storage.
 * e.g.
 *    static std::mutex *myMutex = STATIC_NEW(std::mutex);
 * To destroy an object created in this way, invoke the destructor explicitly, e.g.
 *    myMutex->~mutex();
 * DO NOT use delete.
 * DO NOT use with class UMutex, which has specific support for static instances.
 *
 * STATIC_NEW is intended for use when
 *   - We want a static (or global) object.
 *   - We don't want it to ever be destructed, or to explicitly control destruction,
 *     to avoid use-after-destruction problems.
 *   - We want to avoid an ordinary heap allocated object,
 *     to avoid the possibility of memory allocation failures, and
 *     to avoid memory leak reports, from valgrind, for example.
 * This is defined as a macro rather than a template function because each invocation
 * must define distinct static storage for the object being returned.
 */
#define STATIC_NEW(type) [] () { \
    alignas(type) static char storage[sizeof(type)]; \
    return new(storage) type();} ()

/**
  *  Heap clean up function, called from u_cleanup()
  *    Clears any user heap functions from u_setMemoryFunctions()
  *    Does NOT deallocate any remaining allocated memory.
  */
U_CFUNC UBool
cmemory_cleanup(void);

/**
 * A function called by <TT>uhash_remove</TT>,
 * <TT>uhash_close</TT>, or <TT>uhash_put</TT> to delete
 * an existing key or value.
 * @param obj A key or value stored in a hashtable
 * @see uprv_deleteUObject
 */
typedef void U_CALLCONV UObjectDeleter(void* obj);

/**
 * Deleter for UObject instances.
 * Works for all subclasses of UObject because it has a virtual destructor.
 */
U_CAPI void U_EXPORT2
uprv_deleteUObject(void *obj);

#ifdef __cplusplus

#include <utility>
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

/**
 * "Smart pointer" class, deletes memory via uprv_free().
 * For most methods see the LocalPointerBase base class.
 * Adds operator[] for array item access.
 *
 * @see LocalPointerBase
 */
template<typename T>
class LocalMemory : public LocalPointerBase<T> {
public:
    using LocalPointerBase<T>::operator*;
    using LocalPointerBase<T>::operator->;
    /**
     * Constructor takes ownership.
     * @param p simple pointer to an array of T items that is adopted
     */
    explicit LocalMemory(T *p=NULL) : LocalPointerBase<T>(p) {}
    /**
     * Move constructor, leaves src with isNull().
     * @param src source smart pointer
     */
    LocalMemory(LocalMemory<T> &&src) U_NOEXCEPT : LocalPointerBase<T>(src.ptr) {
        src.ptr=NULL;
    }
    /**
     * Destructor deletes the memory it owns.
     */
    ~LocalMemory() {
        uprv_free(LocalPointerBase<T>::ptr);
    }
    /**
     * Move assignment operator, leaves src with isNull().
     * The behavior is undefined if *this and src are the same object.
     * @param src source smart pointer
     * @return *this
     */
    LocalMemory<T> &operator=(LocalMemory<T> &&src) U_NOEXCEPT {
        uprv_free(LocalPointerBase<T>::ptr);
        LocalPointerBase<T>::ptr=src.ptr;
        src.ptr=NULL;
        return *this;
    }
    /**
     * Swap pointers.
     * @param other other smart pointer
     */
    void swap(LocalMemory<T> &other) U_NOEXCEPT {
        T *temp=LocalPointerBase<T>::ptr;
        LocalPointerBase<T>::ptr=other.ptr;
        other.ptr=temp;
    }
    /**
     * Non-member LocalMemory swap function.
     * @param p1 will get p2's pointer
     * @param p2 will get p1's pointer
     */
    friend inline void swap(LocalMemory<T> &p1, LocalMemory<T> &p2) U_NOEXCEPT {
        p1.swap(p2);
    }
    /**
     * Deletes the array it owns,
     * and adopts (takes ownership of) the one passed in.
     * @param p simple pointer to an array of T items that is adopted
     */
    void adoptInstead(T *p) {
        uprv_free(LocalPointerBase<T>::ptr);
        LocalPointerBase<T>::ptr=p;
    }
    /**
     * Deletes the array it owns, allocates a new one and reset its bytes to 0.
     * Returns the new array pointer.
     * If the allocation fails, then the current array is unchanged and
     * this method returns NULL.
     * @param newCapacity must be >0
     * @return the allocated array pointer, or NULL if the allocation failed
     */
    inline T *allocateInsteadAndReset(int32_t newCapacity=1);
    /**
     * Deletes the array it owns and allocates a new one, copying length T items.
     * Returns the new array pointer.
     * If the allocation fails, then the current array is unchanged and
     * this method returns NULL.
     * @param newCapacity must be >0
     * @param length number of T items to be copied from the old array to the new one;
     *               must be no more than the capacity of the old array,
     *               which the caller must track because the LocalMemory does not track it
     * @return the allocated array pointer, or NULL if the allocation failed
     */
    inline T *allocateInsteadAndCopy(int32_t newCapacity=1, int32_t length=0);
    /**
     * Array item access (writable).
     * No index bounds check.
     * @param i array index
     * @return reference to the array item
     */
    T &operator[](ptrdiff_t i) const { return LocalPointerBase<T>::ptr[i]; }
};

template<typename T>
inline T *LocalMemory<T>::allocateInsteadAndReset(int32_t newCapacity) {
    if(newCapacity>0) {
        T *p=(T *)uprv_malloc(newCapacity*sizeof(T));
        if(p!=NULL) {
            uprv_memset(p, 0, newCapacity*sizeof(T));
            uprv_free(LocalPointerBase<T>::ptr);
            LocalPointerBase<T>::ptr=p;
        }
        return p;
    } else {
        return NULL;
    }
}


template<typename T>
inline T *LocalMemory<T>::allocateInsteadAndCopy(int32_t newCapacity, int32_t length) {
    if(newCapacity>0) {
        T *p=(T *)uprv_malloc(newCapacity*sizeof(T));
        if(p!=NULL) {
            if(length>0) {
                if(length>newCapacity) {
                    length=newCapacity;
                }
                uprv_memcpy(p, LocalPointerBase<T>::ptr, (size_t)length*sizeof(T));
            }
            uprv_free(LocalPointerBase<T>::ptr);
            LocalPointerBase<T>::ptr=p;
        }
        return p;
    } else {
        return NULL;
    }
}

/**
 * Simple array/buffer management class using uprv_malloc() and uprv_free().
 * Provides an internal array with fixed capacity. Can alias another array
 * or allocate one.
 *
 * The array address is properly aligned for type T. It might not be properly
 * aligned for types larger than T (or larger than the largest subtype of T).
 *
 * Unlike LocalMemory and LocalArray, this class never adopts
 * (takes ownership of) another array.
 *
 * WARNING: MaybeStackArray only works with primitive (plain-old data) types.
 * It does NOT know how to call a destructor! If you work with classes with
 * destructors, consider LocalArray in localpointer.h or MemoryPool.
 */
template<typename T, int32_t stackCapacity>
class MaybeStackArray {
public:
    // No heap allocation. Use only on the stack.
    static void* U_EXPORT2 operator new(size_t) U_NOEXCEPT = delete;
    static void* U_EXPORT2 operator new[](size_t) U_NOEXCEPT = delete;
#if U_HAVE_PLACEMENT_NEW
    static void* U_EXPORT2 operator new(size_t, void*) U_NOEXCEPT = delete;
#endif

    /**
     * Default constructor initializes with internal T[stackCapacity] buffer.
     */
    MaybeStackArray() : ptr(stackArray), capacity(stackCapacity), needToRelease(FALSE) {}
    /**
     * Automatically allocates the heap array if the argument is larger than the stack capacity.
     * Intended for use when an approximate capacity is known at compile time but the true
     * capacity is not known until runtime.
     */
    MaybeStackArray(int32_t newCapacity) : MaybeStackArray() {
        if (capacity < newCapacity) { resize(newCapacity); }
    }
    /**
     * Destructor deletes the array (if owned).
     */
    ~MaybeStackArray() { releaseArray(); }
    /**
     * Move constructor: transfers ownership or copies the stack array.
     */
    MaybeStackArray(MaybeStackArray<T, stackCapacity> &&src) U_NOEXCEPT;
    /**
     * Move assignment: transfers ownership or copies the stack array.
     */
    MaybeStackArray<T, stackCapacity> &operator=(MaybeStackArray<T, stackCapacity> &&src) U_NOEXCEPT;
    /**
     * Returns the array capacity (number of T items).
     * @return array capacity
     */
    int32_t getCapacity() const { return capacity; }
    /**
     * Access without ownership change.
     * @return the array pointer
     */
    T *getAlias() const { return ptr; }
    /**
     * Returns the array limit. Simple convenience method.
     * @return getAlias()+getCapacity()
     */
    T *getArrayLimit() const { return getAlias()+capacity; }
    // No "operator T *() const" because that can make
    // expressions like mbs[index] ambiguous for some compilers.
    /**
     * Array item access (const).
     * No index bounds check.
     * @param i array index
     * @return reference to the array item
     */
    const T &operator[](ptrdiff_t i) const { return ptr[i]; }
    /**
     * Array item access (writable).
     * No index bounds check.
     * @param i array index
     * @return reference to the array item
     */
    T &operator[](ptrdiff_t i) { return ptr[i]; }
    /**
     * Deletes the array (if owned) and aliases another one, no transfer of ownership.
     * If the arguments are illegal, then the current array is unchanged.
     * @param otherArray must not be NULL
     * @param otherCapacity must be >0
     */
    void aliasInstead(T *otherArray, int32_t otherCapacity) {
        if(otherArray!=NULL && otherCapacity>0) {
            releaseArray();
            ptr=otherArray;
            capacity=otherCapacity;
            needToRelease=FALSE;
        }
    }
    /**
     * Deletes the array (if owned) and allocates a new one, copying length T items.
     * Returns the new array pointer.
     * If the allocation fails, then the current array is unchanged and
     * this method returns NULL.
     * @param newCapacity can be less than or greater than the current capacity;
     *                    must be >0
     * @param length number of T items to be copied from the old array to the new one
     * @return the allocated array pointer, or NULL if the allocation failed
     */
    inline T *resize(int32_t newCapacity, int32_t length=0);
    /**
     * Gives up ownership of the array if owned, or else clones it,
     * copying length T items; resets itself to the internal stack array.
     * Returns NULL if the allocation failed.
     * @param length number of T items to copy when cloning,
     *        and capacity of the clone when cloning
     * @param resultCapacity will be set to the returned array's capacity (output-only)
     * @return the array pointer;
     *         caller becomes responsible for deleting the array
     */
    inline T *orphanOrClone(int32_t length, int32_t &resultCapacity);
private:
    T *ptr;
    int32_t capacity;
    UBool needToRelease;
    T stackArray[stackCapacity];
    void releaseArray() {
        if(needToRelease) {
            uprv_free(ptr);
        }
    }
    void resetToStackArray() {
        ptr=stackArray;
        capacity=stackCapacity;
        needToRelease=FALSE;
    }
    /* No comparison operators with other MaybeStackArray's. */
    bool operator==(const MaybeStackArray & /*other*/) {return FALSE;}
    bool operator!=(const MaybeStackArray & /*other*/) {return TRUE;}
    /* No ownership transfer: No copy constructor, no assignment operator. */
    MaybeStackArray(const MaybeStackArray & /*other*/) {}
    void operator=(const MaybeStackArray & /*other*/) {}
};

template<typename T, int32_t stackCapacity>
icu::MaybeStackArray<T, stackCapacity>::MaybeStackArray(
        MaybeStackArray <T, stackCapacity>&& src) U_NOEXCEPT
        : ptr(src.ptr), capacity(src.capacity), needToRelease(src.needToRelease) {
    if (src.ptr == src.stackArray) {
        ptr = stackArray;
        uprv_memcpy(stackArray, src.stackArray, sizeof(T) * src.capacity);
    } else {
        src.resetToStackArray();  // take ownership away from src
    }
}

template<typename T, int32_t stackCapacity>
inline MaybeStackArray <T, stackCapacity>&
MaybeStackArray<T, stackCapacity>::operator=(MaybeStackArray <T, stackCapacity>&& src) U_NOEXCEPT {
    releaseArray();  // in case this instance had its own memory allocated
    capacity = src.capacity;
    needToRelease = src.needToRelease;
    if (src.ptr == src.stackArray) {
        ptr = stackArray;
        uprv_memcpy(stackArray, src.stackArray, sizeof(T) * src.capacity);
    } else {
        ptr = src.ptr;
        src.resetToStackArray();  // take ownership away from src
    }
    return *this;
}

template<typename T, int32_t stackCapacity>
inline T *MaybeStackArray<T, stackCapacity>::resize(int32_t newCapacity, int32_t length) {
    if(newCapacity>0) {
#if U_DEBUG && defined(UPRV_MALLOC_COUNT)
      ::fprintf(::stderr,"MaybeStacArray (resize) alloc %d * %lu\n", newCapacity,sizeof(T));
#endif
        T *p=(T *)uprv_malloc(newCapacity*sizeof(T));
        if(p!=NULL) {
            if(length>0) {
                if(length>capacity) {
                    length=capacity;
                }
                if(length>newCapacity) {
                    length=newCapacity;
                }
                uprv_memcpy(p, ptr, (size_t)length*sizeof(T));
            }
            releaseArray();
            ptr=p;
            capacity=newCapacity;
            needToRelease=TRUE;
        }
        return p;
    } else {
        return NULL;
    }
}

template<typename T, int32_t stackCapacity>
inline T *MaybeStackArray<T, stackCapacity>::orphanOrClone(int32_t length, int32_t &resultCapacity) {
    T *p;
    if(needToRelease) {
        p=ptr;
    } else if(length<=0) {
        return NULL;
    } else {
        if(length>capacity) {
            length=capacity;
        }
        p=(T *)uprv_malloc(length*sizeof(T));
#if U_DEBUG && defined(UPRV_MALLOC_COUNT)
      ::fprintf(::stderr,"MaybeStacArray (orphan) alloc %d * %lu\n", length,sizeof(T));
#endif
        if(p==NULL) {
            return NULL;
        }
        uprv_memcpy(p, ptr, (size_t)length*sizeof(T));
    }
    resultCapacity=length;
    resetToStackArray();
    return p;
}

/**
 * Variant of MaybeStackArray that allocates a header struct and an array
 * in one contiguous memory block, using uprv_malloc() and uprv_free().
 * Provides internal memory with fixed array capacity. Can alias another memory
 * block or allocate one.
 * The stackCapacity is the number of T items in the internal memory,
 * not counting the H header.
 * Unlike LocalMemory and LocalArray, this class never adopts
 * (takes ownership of) another memory block.
 */
template<typename H, typename T, int32_t stackCapacity>
class MaybeStackHeaderAndArray {
public:
    // No heap allocation. Use only on the stack.
    static void* U_EXPORT2 operator new(size_t) U_NOEXCEPT = delete;
    static void* U_EXPORT2 operator new[](size_t) U_NOEXCEPT = delete;
#if U_HAVE_PLACEMENT_NEW
    static void* U_EXPORT2 operator new(size_t, void*) U_NOEXCEPT = delete;
#endif

    /**
     * Default constructor initializes with internal H+T[stackCapacity] buffer.
     */
    MaybeStackHeaderAndArray() : ptr(&stackHeader), capacity(stackCapacity), needToRelease(FALSE) {}
    /**
     * Destructor deletes the memory (if owned).
     */
    ~MaybeStackHeaderAndArray() { releaseMemory(); }
    /**
     * Returns the array capacity (number of T items).
     * @return array capacity
     */
    int32_t getCapacity() const { return capacity; }
    /**
     * Access without ownership change.
     * @return the header pointer
     */
    H *getAlias() const { return ptr; }
    /**
     * Returns the array start.
     * @return array start, same address as getAlias()+1
     */
    T *getArrayStart() const { return reinterpret_cast<T *>(getAlias()+1); }
    /**
     * Returns the array limit.
     * @return array limit
     */
    T *getArrayLimit() const { return getArrayStart()+capacity; }
    /**
     * Access without ownership change. Same as getAlias().
     * A class instance can be used directly in expressions that take a T *.
     * @return the header pointer
     */
    operator H *() const { return ptr; }
    /**
     * Array item access (writable).
     * No index bounds check.
     * @param i array index
     * @return reference to the array item
     */
    T &operator[](ptrdiff_t i) { return getArrayStart()[i]; }
    /**
     * Deletes the memory block (if owned) and aliases another one, no transfer of ownership.
     * If the arguments are illegal, then the current memory is unchanged.
     * @param otherArray must not be NULL
     * @param otherCapacity must be >0
     */
    void aliasInstead(H *otherMemory, int32_t otherCapacity) {
        if(otherMemory!=NULL && otherCapacity>0) {
            releaseMemory();
            ptr=otherMemory;
            capacity=otherCapacity;
            needToRelease=FALSE;
        }
    }
    /**
     * Deletes the memory block (if owned) and allocates a new one,
     * copying the header and length T array items.
     * Returns the new header pointer.
     * If the allocation fails, then the current memory is unchanged and
     * this method returns NULL.
     * @param newCapacity can be less than or greater than the current capacity;
     *                    must be >0
     * @param length number of T items to be copied from the old array to the new one
     * @return the allocated pointer, or NULL if the allocation failed
     */
    inline H *resize(int32_t newCapacity, int32_t length=0);
    /**
     * Gives up ownership of the memory if owned, or else clones it,
     * copying the header and length T array items; resets itself to the internal memory.
     * Returns NULL if the allocation failed.
     * @param length number of T items to copy when cloning,
     *        and array capacity of the clone when cloning
     * @param resultCapacity will be set to the returned array's capacity (output-only)
     * @return the header pointer;
     *         caller becomes responsible for deleting the array
     */
    inline H *orphanOrClone(int32_t length, int32_t &resultCapacity);
private:
    H *ptr;
    int32_t capacity;
    UBool needToRelease;
    // stackHeader must precede stackArray immediately.
    H stackHeader;
    T stackArray[stackCapacity];
    void releaseMemory() {
        if(needToRelease) {
            uprv_free(ptr);
        }
    }
    /* No comparison operators with other MaybeStackHeaderAndArray's. */
    bool operator==(const MaybeStackHeaderAndArray & /*other*/) {return FALSE;}
    bool operator!=(const MaybeStackHeaderAndArray & /*other*/) {return TRUE;}
    /* No ownership transfer: No copy constructor, no assignment operator. */
    MaybeStackHeaderAndArray(const MaybeStackHeaderAndArray & /*other*/) {}
    void operator=(const MaybeStackHeaderAndArray & /*other*/) {}
};

template<typename H, typename T, int32_t stackCapacity>
inline H *MaybeStackHeaderAndArray<H, T, stackCapacity>::resize(int32_t newCapacity,
                                                                int32_t length) {
    if(newCapacity>=0) {
#if U_DEBUG && defined(UPRV_MALLOC_COUNT)
      ::fprintf(::stderr,"MaybeStackHeaderAndArray alloc %d + %d * %ul\n", sizeof(H),newCapacity,sizeof(T));
#endif
        H *p=(H *)uprv_malloc(sizeof(H)+newCapacity*sizeof(T));
        if(p!=NULL) {
            if(length<0) {
                length=0;
            } else if(length>0) {
                if(length>capacity) {
                    length=capacity;
                }
                if(length>newCapacity) {
                    length=newCapacity;
                }
            }
            uprv_memcpy(p, ptr, sizeof(H)+(size_t)length*sizeof(T));
            releaseMemory();
            ptr=p;
            capacity=newCapacity;
            needToRelease=TRUE;
        }
        return p;
    } else {
        return NULL;
    }
}

template<typename H, typename T, int32_t stackCapacity>
inline H *MaybeStackHeaderAndArray<H, T, stackCapacity>::orphanOrClone(int32_t length,
                                                                       int32_t &resultCapacity) {
    H *p;
    if(needToRelease) {
        p=ptr;
    } else {
        if(length<0) {
            length=0;
        } else if(length>capacity) {
            length=capacity;
        }
#if U_DEBUG && defined(UPRV_MALLOC_COUNT)
      ::fprintf(::stderr,"MaybeStackHeaderAndArray (orphan) alloc %ul + %d * %lu\n", sizeof(H),length,sizeof(T));
#endif
        p=(H *)uprv_malloc(sizeof(H)+length*sizeof(T));
        if(p==NULL) {
            return NULL;
        }
        uprv_memcpy(p, ptr, sizeof(H)+(size_t)length*sizeof(T));
    }
    resultCapacity=length;
    ptr=&stackHeader;
    capacity=stackCapacity;
    needToRelease=FALSE;
    return p;
}

/**
 * A simple memory management class that creates new heap allocated objects (of
 * any class that has a public constructor), keeps track of them and eventually
 * deletes them all in its own destructor.
 *
 * A typical use-case would be code like this:
 *
 *     MemoryPool<MyType> pool;
 *
 *     MyType* o1 = pool.create();
 *     if (o1 != nullptr) {
 *         foo(o1);
 *     }
 *
 *     MyType* o2 = pool.create(1, 2, 3);
 *     if (o2 != nullptr) {
 *         bar(o2);
 *     }
 *
 *     // MemoryPool will take care of deleting the MyType objects.
 *
 * It doesn't do anything more than that, and is intentionally kept minimalist.
 */
template<typename T, int32_t stackCapacity = 8>
class MemoryPool : public UMemory {
public:
    MemoryPool() : count(0), pool() {}

    ~MemoryPool() {
        for (int32_t i = 0; i < count; ++i) {
            delete pool[i];
        }
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    MemoryPool(MemoryPool&& other) U_NOEXCEPT : count(other.count),
                                                pool(std::move(other.pool)) {
        other.count = 0;
    }

    MemoryPool& operator=(MemoryPool&& other) U_NOEXCEPT {
        count = other.count;
        pool = std::move(other.pool);
        other.count = 0;
        return *this;
    }

    /**
     * Creates a new object of typename T, by forwarding any and all arguments
     * to the typename T constructor.
     *
     * @param args Arguments to be forwarded to the typename T constructor.
     * @return A pointer to the newly created object, or nullptr on error.
     */
    template<typename... Args>
    T* create(Args&&... args) {
        int32_t capacity = pool.getCapacity();
        if (count == capacity &&
            pool.resize(capacity == stackCapacity ? 4 * capacity : 2 * capacity,
                        capacity) == nullptr) {
            return nullptr;
        }
        return pool[count++] = new T(std::forward<Args>(args)...);
    }

private:
    int32_t count;
    MaybeStackArray<T*, stackCapacity> pool;
};

U_NAMESPACE_END

#endif  /* __cplusplus */
#endif  /* CMEMORY_H */
