// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformSTL_h
#define PlatformSTL_h

#include <memory>

#define PLATFORM_EXPORT
#ifndef CHECK
#define CHECK(condition) ((void) 0)
#endif
#define DCHECK(condition) ((void) 0)
#define NOTREACHED()
#define DCHECK_EQ(i, j) DCHECK(i == j)
#define DCHECK_GE(i, j) DCHECK(i >= j)
#define DCHECK_LT(i, j) DCHECK(i < j)
#define DCHECK_GT(i, j) DCHECK(i > j)
template <typename T>
inline void USE(T) { }

#define DEFINE_STATIC_LOCAL(type, name, arguments) \
    static type name;

#if defined(__APPLE__) && !defined(_LIBCPP_VERSION)

namespace std {

template <typename T1, typename T2>
struct is_convertible {
private:
    struct True_ {
        char x[2];
    };
    struct False_ {
    };

    static True_ helper(T2 const &);
    static False_ helper(...);

public:
    static bool const value = (
        sizeof(True_) == sizeof(is_convertible::helper(T1()))
    );
};

template <bool, class T = void>
struct enable_if {
};

template <class T>
struct enable_if<true, T> {
    typedef T type;
};

template<class T>
struct remove_extent {
    typedef T type;
};

template<class T>
struct remove_extent<T[]> {
    typedef T type;
};

template<class T, std::size_t N>
struct remove_extent<T[N]> {
    typedef T type;
};

typedef decltype(nullptr) nullptr_t;

template<class T, T v>
struct integral_constant {
    static constexpr T value = v;
    typedef T value_type;
    typedef integral_constant type;
    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};

typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

template<class T>
struct is_array : false_type {};

template<class T>
struct is_array<T[]> : true_type {};

template<class T, std::size_t N>
struct is_array<T[N]> : true_type {};

template <typename T>
struct OwnedPtrDeleter {
    static void deletePtr(T* ptr)
    {
        static_assert(sizeof(T) > 0, "type must be complete");
        delete ptr;
    }
};

template <typename T>
struct OwnedPtrDeleter<T[]> {
    static void deletePtr(T* ptr)
    {
        static_assert(sizeof(T) > 0, "type must be complete");
        delete[] ptr;
    }
};

template <class T, int n>
struct OwnedPtrDeleter<T[n]> {
    static_assert(sizeof(T) < 0, "do not use array with size as type");
};

template <typename T> class unique_ptr {
public:
    typedef typename remove_extent<T>::type ValueType;
    typedef ValueType* PtrType;

    unique_ptr() : m_ptr(nullptr) {}
    unique_ptr(std::nullptr_t) : m_ptr(nullptr) {}
    unique_ptr(unique_ptr&&);
    template <typename U, typename = typename enable_if<is_convertible<U*, T*>::value>::type> unique_ptr(unique_ptr<U>&&);

    ~unique_ptr()
    {
        OwnedPtrDeleter<T>::deletePtr(m_ptr);
        m_ptr = nullptr;
    }

    PtrType get() const { return m_ptr; }

    void reset(PtrType = nullptr);
    PtrType release();

    ValueType& operator*() const { DCHECK(m_ptr); return *m_ptr; }
    PtrType operator->() const { DCHECK(m_ptr); return m_ptr; }

    ValueType& operator[](std::ptrdiff_t i) const;

    bool operator!() const { return !m_ptr; }
    explicit operator bool() const { return m_ptr; }

    unique_ptr& operator=(std::nullptr_t) { reset(); return *this; }


    unique_ptr& operator=(unique_ptr&&);
    template <typename U> unique_ptr& operator=(unique_ptr<U>&&);

    void swap(unique_ptr& o) { std::swap(m_ptr, o.m_ptr); }

    static T* hashTableDeletedValue() { return reinterpret_cast<T*>(-1); }

    explicit unique_ptr(PtrType ptr) : m_ptr(ptr) {}

private:

    // We should never have two unique_ptrs for the same underlying object
    // (otherwise we'll get double-destruction), so these equality operators
    // should never be needed.
    template <typename U> bool operator==(const unique_ptr<U>&) const
    {
        static_assert(!sizeof(U*), "unique_ptrs should never be equal");
        return false;
    }
    template <typename U> bool operator!=(const unique_ptr<U>&) const
    {
        static_assert(!sizeof(U*), "unique_ptrs should never be equal");
        return false;
    }

    PtrType m_ptr;
};


template <typename T> inline void unique_ptr<T>::reset(PtrType ptr)
{
    PtrType p = m_ptr;
    m_ptr = ptr;
    OwnedPtrDeleter<T>::deletePtr(p);
}

template <typename T> inline typename unique_ptr<T>::PtrType unique_ptr<T>::release()
{
    PtrType ptr = m_ptr;
    m_ptr = nullptr;
    return ptr;
}

template <typename T> inline typename unique_ptr<T>::ValueType& unique_ptr<T>::operator[](std::ptrdiff_t i) const
{
    static_assert(is_array<T>::value, "elements access is possible for arrays only");
    DCHECK(m_ptr);
    DCHECK_GE(i, 0);
    return m_ptr[i];
}

template <typename T> inline unique_ptr<T>::unique_ptr(unique_ptr<T>&& o)
    : m_ptr(o.release())
{
}

template <typename T>
template <typename U, typename> inline unique_ptr<T>::unique_ptr(unique_ptr<U>&& o)
    : m_ptr(o.release())
{
    static_assert(!is_array<T>::value, "pointers to array must never be converted");
}

template <typename T> inline unique_ptr<T>& unique_ptr<T>::operator=(unique_ptr<T>&& o)
{
    PtrType ptr = m_ptr;
    m_ptr = o.release();
    DCHECK(!ptr || m_ptr != ptr);
    OwnedPtrDeleter<T>::deletePtr(ptr);

    return *this;
}

template <typename T>
template <typename U> inline unique_ptr<T>& unique_ptr<T>::operator=(unique_ptr<U>&& o)
{
    static_assert(!is_array<T>::value, "pointers to array must never be converted");
    PtrType ptr = m_ptr;
    m_ptr = o.release();
    DCHECK(!ptr || m_ptr != ptr);
    OwnedPtrDeleter<T>::deletePtr(ptr);

    return *this;
}

template <typename T> inline void swap(unique_ptr<T>& a, unique_ptr<T>& b)
{
    a.swap(b);
}

template <typename T, typename U> inline bool operator==(const unique_ptr<T>& a, U* b)
{
    return a.get() == b;
}

template <typename T, typename U> inline bool operator==(T* a, const unique_ptr<U>& b)
{
    return a == b.get();
}

template <typename T, typename U> inline bool operator!=(const unique_ptr<T>& a, U* b)
{
    return a.get() != b;
}

template <typename T, typename U> inline bool operator!=(T* a, const unique_ptr<U>& b)
{
    return a != b.get();
}

template <typename T> inline typename unique_ptr<T>::PtrType getPtr(const unique_ptr<T>& p)
{
    return p.get();
}

template <typename T>
unique_ptr<T> move(unique_ptr<T>& ptr)
{
    return unique_ptr<T>(ptr.release());
}

}

#endif // defined(__APPLE__) && !defined(_LIBCPP_VERSION)

template <typename T>
std::unique_ptr<T> wrapUnique(T* ptr)
{
    return std::unique_ptr<T>(ptr);
}

#endif // PlatformSTL_h
