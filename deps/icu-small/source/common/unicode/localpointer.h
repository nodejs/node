/*
*******************************************************************************
*
*   Copyright (C) 2009-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  localpointer.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov13
*   created by: Markus W. Scherer
*/

#ifndef __LOCALPOINTER_H__
#define __LOCALPOINTER_H__

/**
 * \file
 * \brief C++ API: "Smart pointers" for use with and in ICU4C C++ code.
 *
 * These classes are inspired by
 * - std::auto_ptr
 * - boost::scoped_ptr & boost::scoped_array
 * - Taligent Safe Pointers (TOnlyPointerTo)
 *
 * but none of those provide for all of the goals for ICU smart pointers:
 * - Smart pointer owns the object and releases it when it goes out of scope.
 * - No transfer of ownership via copy/assignment to reduce misuse. Simpler & more robust.
 * - ICU-compatible: No exceptions.
 * - Need to be able to orphan/release the pointer and its ownership.
 * - Need variants for normal C++ object pointers, C++ arrays, and ICU C service objects.
 *
 * For details see http://site.icu-project.org/design/cpp/scoped_ptr
 */

#include "unicode/utypes.h"

#if U_SHOW_CPLUSPLUS_API

U_NAMESPACE_BEGIN

/**
 * "Smart pointer" base class; do not use directly: use LocalPointer etc.
 *
 * Base class for smart pointer classes that do not throw exceptions.
 *
 * Do not use this base class directly, since it does not delete its pointer.
 * A subclass must implement methods that delete the pointer:
 * Destructor and adoptInstead().
 *
 * There is no operator T *() provided because the programmer must decide
 * whether to use getAlias() (without transfer of ownership) or orphan()
 * (with transfer of ownership and NULLing of the pointer).
 *
 * @see LocalPointer
 * @see LocalArray
 * @see U_DEFINE_LOCAL_OPEN_POINTER
 * @stable ICU 4.4
 */
template<typename T>
class LocalPointerBase {
public:
    /**
     * Constructor takes ownership.
     * @param p simple pointer to an object that is adopted
     * @stable ICU 4.4
     */
    explicit LocalPointerBase(T *p=NULL) : ptr(p) {}
    /**
     * Destructor deletes the object it owns.
     * Subclass must override: Base class does nothing.
     * @stable ICU 4.4
     */
    ~LocalPointerBase() { /* delete ptr; */ }
    /**
     * NULL check.
     * @return TRUE if ==NULL
     * @stable ICU 4.4
     */
    UBool isNull() const { return ptr==NULL; }
    /**
     * NULL check.
     * @return TRUE if !=NULL
     * @stable ICU 4.4
     */
    UBool isValid() const { return ptr!=NULL; }
    /**
     * Comparison with a simple pointer, so that existing code
     * with ==NULL need not be changed.
     * @param other simple pointer for comparison
     * @return true if this pointer value equals other
     * @stable ICU 4.4
     */
    bool operator==(const T *other) const { return ptr==other; }
    /**
     * Comparison with a simple pointer, so that existing code
     * with !=NULL need not be changed.
     * @param other simple pointer for comparison
     * @return true if this pointer value differs from other
     * @stable ICU 4.4
     */
    bool operator!=(const T *other) const { return ptr!=other; }
    /**
     * Access without ownership change.
     * @return the pointer value
     * @stable ICU 4.4
     */
    T *getAlias() const { return ptr; }
    /**
     * Access without ownership change.
     * @return the pointer value as a reference
     * @stable ICU 4.4
     */
    T &operator*() const { return *ptr; }
    /**
     * Access without ownership change.
     * @return the pointer value
     * @stable ICU 4.4
     */
    T *operator->() const { return ptr; }
    /**
     * Gives up ownership; the internal pointer becomes NULL.
     * @return the pointer value;
     *         caller becomes responsible for deleting the object
     * @stable ICU 4.4
     */
    T *orphan() {
        T *p=ptr;
        ptr=NULL;
        return p;
    }
    /**
     * Deletes the object it owns,
     * and adopts (takes ownership of) the one passed in.
     * Subclass must override: Base class does not delete the object.
     * @param p simple pointer to an object that is adopted
     * @stable ICU 4.4
     */
    void adoptInstead(T *p) {
        // delete ptr;
        ptr=p;
    }
protected:
    /**
     * Actual pointer.
     * @internal
     */
    T *ptr;
private:
    // No comparison operators with other LocalPointerBases.
    bool operator==(const LocalPointerBase<T> &other);
    bool operator!=(const LocalPointerBase<T> &other);
    // No ownership sharing: No copy constructor, no assignment operator.
    LocalPointerBase(const LocalPointerBase<T> &other);
    void operator=(const LocalPointerBase<T> &other);
    // No heap allocation. Use only on the stack.
    static void * U_EXPORT2 operator new(size_t size);
    static void * U_EXPORT2 operator new[](size_t size);
#if U_HAVE_PLACEMENT_NEW
    static void * U_EXPORT2 operator new(size_t, void *ptr);
#endif
};

/**
 * "Smart pointer" class, deletes objects via the standard C++ delete operator.
 * For most methods see the LocalPointerBase base class.
 *
 * Usage example:
 * \code
 * LocalPointer<UnicodeString> s(new UnicodeString((UChar32)0x50005));
 * int32_t length=s->length();  // 2
 * UChar lead=s->charAt(0);  // 0xd900
 * if(some condition) { return; }  // no need to explicitly delete the pointer
 * s.adoptInstead(new UnicodeString((UChar)0xfffc));
 * length=s->length();  // 1
 * // no need to explicitly delete the pointer
 * \endcode
 *
 * @see LocalPointerBase
 * @stable ICU 4.4
 */
template<typename T>
class LocalPointer : public LocalPointerBase<T> {
public:
    using LocalPointerBase<T>::operator*;
    using LocalPointerBase<T>::operator->;
    /**
     * Constructor takes ownership.
     * @param p simple pointer to an object that is adopted
     * @stable ICU 4.4
     */
    explicit LocalPointer(T *p=NULL) : LocalPointerBase<T>(p) {}
    /**
     * Constructor takes ownership and reports an error if NULL.
     *
     * This constructor is intended to be used with other-class constructors
     * that may report a failure UErrorCode,
     * so that callers need to check only for U_FAILURE(errorCode)
     * and not also separately for isNull().
     *
     * @param p simple pointer to an object that is adopted
     * @param errorCode in/out UErrorCode, set to U_MEMORY_ALLOCATION_ERROR
     *     if p==NULL and no other failure code had been set
     * @stable ICU 55
     */
    LocalPointer(T *p, UErrorCode &errorCode) : LocalPointerBase<T>(p) {
        if(p==NULL && U_SUCCESS(errorCode)) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
        }
    }
#ifndef U_HIDE_DRAFT_API
#if U_HAVE_RVALUE_REFERENCES
    /**
     * Move constructor, leaves src with isNull().
     * @param src source smart pointer
     * @draft ICU 56
     */
    LocalPointer(LocalPointer<T> &&src) U_NOEXCEPT : LocalPointerBase<T>(src.ptr) {
        src.ptr=NULL;
    }
#endif
#endif  /* U_HIDE_DRAFT_API */
    /**
     * Destructor deletes the object it owns.
     * @stable ICU 4.4
     */
    ~LocalPointer() {
        delete LocalPointerBase<T>::ptr;
    }
#ifndef U_HIDE_DRAFT_API
#if U_HAVE_RVALUE_REFERENCES
    /**
     * Move assignment operator, leaves src with isNull().
     * The behavior is undefined if *this and src are the same object.
     * @param src source smart pointer
     * @return *this
     * @draft ICU 56
     */
    LocalPointer<T> &operator=(LocalPointer<T> &&src) U_NOEXCEPT {
        return moveFrom(src);
    }
#endif
    /**
     * Move assignment, leaves src with isNull().
     * The behavior is undefined if *this and src are the same object.
     *
     * Can be called explicitly, does not need C++11 support.
     * @param src source smart pointer
     * @return *this
     * @draft ICU 56
     */
    LocalPointer<T> &moveFrom(LocalPointer<T> &src) U_NOEXCEPT {
        delete LocalPointerBase<T>::ptr;
        LocalPointerBase<T>::ptr=src.ptr;
        src.ptr=NULL;
        return *this;
    }
    /**
     * Swap pointers.
     * @param other other smart pointer
     * @draft ICU 56
     */
    void swap(LocalPointer<T> &other) U_NOEXCEPT {
        T *temp=LocalPointerBase<T>::ptr;
        LocalPointerBase<T>::ptr=other.ptr;
        other.ptr=temp;
    }
#endif  /* U_HIDE_DRAFT_API */
    /**
     * Non-member LocalPointer swap function.
     * @param p1 will get p2's pointer
     * @param p2 will get p1's pointer
     * @draft ICU 56
     */
    friend inline void swap(LocalPointer<T> &p1, LocalPointer<T> &p2) U_NOEXCEPT {
        p1.swap(p2);
    }
    /**
     * Deletes the object it owns,
     * and adopts (takes ownership of) the one passed in.
     * @param p simple pointer to an object that is adopted
     * @stable ICU 4.4
     */
    void adoptInstead(T *p) {
        delete LocalPointerBase<T>::ptr;
        LocalPointerBase<T>::ptr=p;
    }
    /**
     * Deletes the object it owns,
     * and adopts (takes ownership of) the one passed in.
     *
     * If U_FAILURE(errorCode), then the current object is retained and the new one deleted.
     *
     * If U_SUCCESS(errorCode) but the input pointer is NULL,
     * then U_MEMORY_ALLOCATION_ERROR is set,
     * the current object is deleted, and NULL is set.
     *
     * @param p simple pointer to an object that is adopted
     * @param errorCode in/out UErrorCode, set to U_MEMORY_ALLOCATION_ERROR
     *     if p==NULL and no other failure code had been set
     * @stable ICU 55
     */
    void adoptInsteadAndCheckErrorCode(T *p, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            delete LocalPointerBase<T>::ptr;
            LocalPointerBase<T>::ptr=p;
            if(p==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
            }
        } else {
            delete p;
        }
    }
};

/**
 * "Smart pointer" class, deletes objects via the C++ array delete[] operator.
 * For most methods see the LocalPointerBase base class.
 * Adds operator[] for array item access.
 *
 * Usage example:
 * \code
 * LocalArray<UnicodeString> a(new UnicodeString[2]);
 * a[0].append((UChar)0x61);
 * if(some condition) { return; }  // no need to explicitly delete the array
 * a.adoptInstead(new UnicodeString[4]);
 * a[3].append((UChar)0x62).append((UChar)0x63).reverse();
 * // no need to explicitly delete the array
 * \endcode
 *
 * @see LocalPointerBase
 * @stable ICU 4.4
 */
template<typename T>
class LocalArray : public LocalPointerBase<T> {
public:
    using LocalPointerBase<T>::operator*;
    using LocalPointerBase<T>::operator->;
    /**
     * Constructor takes ownership.
     * @param p simple pointer to an array of T objects that is adopted
     * @stable ICU 4.4
     */
    explicit LocalArray(T *p=NULL) : LocalPointerBase<T>(p) {}
#ifndef U_HIDE_DRAFT_API
    /**
     * Constructor takes ownership and reports an error if NULL.
     *
     * This constructor is intended to be used with other-class constructors
     * that may report a failure UErrorCode,
     * so that callers need to check only for U_FAILURE(errorCode)
     * and not also separately for isNull().
     *
     * @param p simple pointer to an array of T objects that is adopted
     * @param errorCode in/out UErrorCode, set to U_MEMORY_ALLOCATION_ERROR
     *     if p==NULL and no other failure code had been set
     * @draft ICU 56
     */
    LocalArray(T *p, UErrorCode &errorCode) : LocalPointerBase<T>(p) {
        if(p==NULL && U_SUCCESS(errorCode)) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
        }
    }
#if U_HAVE_RVALUE_REFERENCES
    /**
     * Move constructor, leaves src with isNull().
     * @param src source smart pointer
     * @draft ICU 56
     */
    LocalArray(LocalArray<T> &&src) U_NOEXCEPT : LocalPointerBase<T>(src.ptr) {
        src.ptr=NULL;
    }
#endif
#endif  /* U_HIDE_DRAFT_API */
    /**
     * Destructor deletes the array it owns.
     * @stable ICU 4.4
     */
    ~LocalArray() {
        delete[] LocalPointerBase<T>::ptr;
    }
#ifndef U_HIDE_DRAFT_API
#if U_HAVE_RVALUE_REFERENCES
    /**
     * Move assignment operator, leaves src with isNull().
     * The behavior is undefined if *this and src are the same object.
     * @param src source smart pointer
     * @return *this
     * @draft ICU 56
     */
    LocalArray<T> &operator=(LocalArray<T> &&src) U_NOEXCEPT {
        return moveFrom(src);
    }
#endif
    /**
     * Move assignment, leaves src with isNull().
     * The behavior is undefined if *this and src are the same object.
     *
     * Can be called explicitly, does not need C++11 support.
     * @param src source smart pointer
     * @return *this
     * @draft ICU 56
     */
    LocalArray<T> &moveFrom(LocalArray<T> &src) U_NOEXCEPT {
        delete[] LocalPointerBase<T>::ptr;
        LocalPointerBase<T>::ptr=src.ptr;
        src.ptr=NULL;
        return *this;
    }
    /**
     * Swap pointers.
     * @param other other smart pointer
     * @draft ICU 56
     */
    void swap(LocalArray<T> &other) U_NOEXCEPT {
        T *temp=LocalPointerBase<T>::ptr;
        LocalPointerBase<T>::ptr=other.ptr;
        other.ptr=temp;
    }
#endif  /* U_HIDE_DRAFT_API */
    /**
     * Non-member LocalArray swap function.
     * @param p1 will get p2's pointer
     * @param p2 will get p1's pointer
     * @draft ICU 56
     */
    friend inline void swap(LocalArray<T> &p1, LocalArray<T> &p2) U_NOEXCEPT {
        p1.swap(p2);
    }
    /**
     * Deletes the array it owns,
     * and adopts (takes ownership of) the one passed in.
     * @param p simple pointer to an array of T objects that is adopted
     * @stable ICU 4.4
     */
    void adoptInstead(T *p) {
        delete[] LocalPointerBase<T>::ptr;
        LocalPointerBase<T>::ptr=p;
    }
#ifndef U_HIDE_DRAFT_API
    /**
     * Deletes the array it owns,
     * and adopts (takes ownership of) the one passed in.
     *
     * If U_FAILURE(errorCode), then the current array is retained and the new one deleted.
     *
     * If U_SUCCESS(errorCode) but the input pointer is NULL,
     * then U_MEMORY_ALLOCATION_ERROR is set,
     * the current array is deleted, and NULL is set.
     *
     * @param p simple pointer to an array of T objects that is adopted
     * @param errorCode in/out UErrorCode, set to U_MEMORY_ALLOCATION_ERROR
     *     if p==NULL and no other failure code had been set
     * @draft ICU 56
     */
    void adoptInsteadAndCheckErrorCode(T *p, UErrorCode &errorCode) {
        if(U_SUCCESS(errorCode)) {
            delete[] LocalPointerBase<T>::ptr;
            LocalPointerBase<T>::ptr=p;
            if(p==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
            }
        } else {
            delete[] p;
        }
    }
#endif  /* U_HIDE_DRAFT_API */
    /**
     * Array item access (writable).
     * No index bounds check.
     * @param i array index
     * @return reference to the array item
     * @stable ICU 4.4
     */
    T &operator[](ptrdiff_t i) const { return LocalPointerBase<T>::ptr[i]; }
};

/**
 * \def U_DEFINE_LOCAL_OPEN_POINTER
 * "Smart pointer" definition macro, deletes objects via the closeFunction.
 * Defines a subclass of LocalPointerBase which works just
 * like LocalPointer<Type> except that this subclass will use the closeFunction
 * rather than the C++ delete operator.
 *
 * Requirement: The closeFunction must tolerate a NULL pointer.
 * (We could add a NULL check here but it is normally redundant.)
 *
 * Usage example:
 * \code
 * LocalUCaseMapPointer csm(ucasemap_open(localeID, options, &errorCode));
 * utf8OutLength=ucasemap_utf8ToLower(csm.getAlias(),
 *     utf8Out, (int32_t)sizeof(utf8Out),
 *     utf8In, utf8InLength, &errorCode);
 * if(U_FAILURE(errorCode)) { return; }  // no need to explicitly delete the UCaseMap
 * \endcode
 *
 * @see LocalPointerBase
 * @see LocalPointer
 * @stable ICU 4.4
 */
#if U_HAVE_RVALUE_REFERENCES
#define U_DEFINE_LOCAL_OPEN_POINTER(LocalPointerClassName, Type, closeFunction) \
    class LocalPointerClassName : public LocalPointerBase<Type> { \
    public: \
        using LocalPointerBase<Type>::operator*; \
        using LocalPointerBase<Type>::operator->; \
        explicit LocalPointerClassName(Type *p=NULL) : LocalPointerBase<Type>(p) {} \
        LocalPointerClassName(LocalPointerClassName &&src) U_NOEXCEPT \
                : LocalPointerBase<Type>(src.ptr) { \
            src.ptr=NULL; \
        } \
        ~LocalPointerClassName() { closeFunction(ptr); } \
        LocalPointerClassName &operator=(LocalPointerClassName &&src) U_NOEXCEPT { \
            return moveFrom(src); \
        } \
        LocalPointerClassName &moveFrom(LocalPointerClassName &src) U_NOEXCEPT { \
            closeFunction(ptr); \
            LocalPointerBase<Type>::ptr=src.ptr; \
            src.ptr=NULL; \
            return *this; \
        } \
        void swap(LocalPointerClassName &other) U_NOEXCEPT { \
            Type *temp=LocalPointerBase<Type>::ptr; \
            LocalPointerBase<Type>::ptr=other.ptr; \
            other.ptr=temp; \
        } \
        friend inline void swap(LocalPointerClassName &p1, LocalPointerClassName &p2) U_NOEXCEPT { \
            p1.swap(p2); \
        } \
        void adoptInstead(Type *p) { \
            closeFunction(ptr); \
            ptr=p; \
        } \
    }
#else
#define U_DEFINE_LOCAL_OPEN_POINTER(LocalPointerClassName, Type, closeFunction) \
    class LocalPointerClassName : public LocalPointerBase<Type> { \
    public: \
        using LocalPointerBase<Type>::operator*; \
        using LocalPointerBase<Type>::operator->; \
        explicit LocalPointerClassName(Type *p=NULL) : LocalPointerBase<Type>(p) {} \
        ~LocalPointerClassName() { closeFunction(ptr); } \
        LocalPointerClassName &moveFrom(LocalPointerClassName &src) U_NOEXCEPT { \
            closeFunction(ptr); \
            LocalPointerBase<Type>::ptr=src.ptr; \
            src.ptr=NULL; \
            return *this; \
        } \
        void swap(LocalPointerClassName &other) U_NOEXCEPT { \
            Type *temp=LocalPointerBase<Type>::ptr; \
            LocalPointerBase<Type>::ptr=other.ptr; \
            other.ptr=temp; \
        } \
        friend inline void swap(LocalPointerClassName &p1, LocalPointerClassName &p2) U_NOEXCEPT { \
            p1.swap(p2); \
        } \
        void adoptInstead(Type *p) { \
            closeFunction(ptr); \
            ptr=p; \
        } \
    }
#endif

U_NAMESPACE_END

#endif  /* U_SHOW_CPLUSPLUS_API */
#endif  /* __LOCALPOINTER_H__ */
