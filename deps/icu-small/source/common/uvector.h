// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1999-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   10/22/99    alan        Creation.  This is an internal header.
*                           It should not be exported.
**********************************************************************
*/

#ifndef UVECTOR_H
#define UVECTOR_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "cmemory.h"
#include "uarrsort.h"
#include "uelement.h"

U_NAMESPACE_BEGIN

/**
 * <p>Ultralightweight C++ implementation of a <tt>void*</tt> vector
 * that is (mostly) compatible with java.util.Vector.
 *
 * <p>This is a very simple implementation, written to satisfy an
 * immediate porting need.  As such, it is not completely fleshed out,
 * and it aims for simplicity and conformity.  Nonetheless, it serves
 * its purpose (porting code from java that uses java.util.Vector)
 * well, and it could be easily made into a more robust vector class.
 *
 * <p><b>Design notes</b>
 *
 * <p>There is index bounds checking, but little is done about it.  If
 * indices are out of bounds, either nothing happens, or zero is
 * returned.  We <em>do</em> avoid indexing off into the weeds.
 *
 * <p>There is detection of out of memory, but the handling is very
 * coarse-grained -- similar to UnicodeString's protocol, but even
 * coarser.  The class contains <em>one static flag</em> that is set
 * when any call to <tt>new</tt> returns zero.  This allows the caller
 * to use several vectors and make just one check at the end to see if
 * a memory failure occurred.  This is more efficient than making a
 * check after each call on each vector when doing many operations on
 * multiple vectors.  The single static flag works best when memory
 * failures are infrequent, and when recovery options are limited or
 * nonexistent.
 *
 * <p>Since we don't have garbage collection, UVector was given the
 * option to <em>own</em>its contents.  To employ this, set a deleter
 * function.  The deleter is called on a void* pointer when that
 * pointer is released by the vector, either when the vector itself is
 * destructed, or when a call to setElementAt() overwrites an element,
 * or when a call to remove() or one of its variants explicitly
 * removes an element.  If no deleter is set, or the deleter is set to
 * zero, then it is assumed that the caller will delete elements as
 * needed.
 *
 * <p>In order to implement methods such as contains() and indexOf(),
 * UVector needs a way to compare objects for equality.  To do so, it
 * uses a comparison function, or "comparer."  If the comparer is not
 * set, or is set to zero, then all such methods will act as if the
 * vector contains no element.  That is, indexOf() will always return
 * -1, contains() will always return FALSE, etc.
 *
 * <p><b>To do</b>
 *
 * <p>Improve the handling of index out of bounds errors.
 *
 * @author Alan Liu
 */
class U_COMMON_API UVector : public UObject {
    // NOTE: UVector uses the UHashKey (union of void* and int32_t) as
    // its basic storage type.  It uses UElementsAreEqual as its
    // comparison function.  It uses UObjectDeleter as its deleter
    // function.  These are named for hashtables, but used here as-is
    // rather than duplicating the type.  This allows sharing of
    // support functions.

private:
    int32_t count;

    int32_t capacity;

    UElement* elements;

    UObjectDeleter *deleter;

    UElementsAreEqual *comparer;

public:
    UVector(UErrorCode &status);

    UVector(int32_t initialCapacity, UErrorCode &status);

    UVector(UObjectDeleter *d, UElementsAreEqual *c, UErrorCode &status);

    UVector(UObjectDeleter *d, UElementsAreEqual *c, int32_t initialCapacity, UErrorCode &status);

    virtual ~UVector();

    /**
     * Assign this object to another (make this a copy of 'other').
     * Use the 'assign' function to assign each element.
     */
    void assign(const UVector& other, UElementAssigner *assign, UErrorCode &ec);

    /**
     * Compare this vector with another.  They will be considered
     * equal if they are of the same size and all elements are equal,
     * as compared using this object's comparer.
     */
    UBool operator==(const UVector& other);

    /**
     * Equivalent to !operator==()
     */
    inline UBool operator!=(const UVector& other);

    //------------------------------------------------------------
    // java.util.Vector API
    //------------------------------------------------------------

    void addElement(void* obj, UErrorCode &status);

    void addElement(int32_t elem, UErrorCode &status);

    void setElementAt(void* obj, int32_t index);

    void setElementAt(int32_t elem, int32_t index);

    void insertElementAt(void* obj, int32_t index, UErrorCode &status);

    void insertElementAt(int32_t elem, int32_t index, UErrorCode &status);

    void* elementAt(int32_t index) const;

    int32_t elementAti(int32_t index) const;

    UBool equals(const UVector &other) const;

    void* firstElement(void) const;

    void* lastElement(void) const;

    int32_t lastElementi(void) const;

    int32_t indexOf(void* obj, int32_t startIndex = 0) const;

    int32_t indexOf(int32_t obj, int32_t startIndex = 0) const;

    UBool contains(void* obj) const;

    UBool contains(int32_t obj) const;

    UBool containsAll(const UVector& other) const;

    UBool removeAll(const UVector& other);

    UBool retainAll(const UVector& other);

    void removeElementAt(int32_t index);

    UBool removeElement(void* obj);

    void removeAllElements();

    int32_t size(void) const;

    UBool isEmpty(void) const;

    UBool ensureCapacity(int32_t minimumCapacity, UErrorCode &status);

    /**
     * Change the size of this vector as follows: If newSize is
     * smaller, then truncate the array, possibly deleting held
     * elements for i >= newSize.  If newSize is larger, grow the
     * array, filling in new slots with NULL.
     */
    void setSize(int32_t newSize, UErrorCode &status);

    /**
     * Fill in the given array with all elements of this vector.
     */
    void** toArray(void** result) const;

    //------------------------------------------------------------
    // New API
    //------------------------------------------------------------

    UObjectDeleter *setDeleter(UObjectDeleter *d);

    UElementsAreEqual *setComparer(UElementsAreEqual *c);

    void* operator[](int32_t index) const;

    /**
     * Removes the element at the given index from this vector and
     * transfer ownership of it to the caller.  After this call, the
     * caller owns the result and must delete it and the vector entry
     * at 'index' is removed, shifting all subsequent entries back by
     * one index and shortening the size of the vector by one.  If the
     * index is out of range or if there is no item at the given index
     * then 0 is returned and the vector is unchanged.
     */
    void* orphanElementAt(int32_t index);

    /**
     * Returns true if this vector contains none of the elements
     * of the given vector.
     * @param other vector to be checked for containment
     * @return true if the test condition is met
     */
    UBool containsNone(const UVector& other) const;

    /**
     * Insert the given object into this vector at its sorted position
     * as defined by 'compare'.  The current elements are assumed to
     * be sorted already.
     */
    void sortedInsert(void* obj, UElementComparator *compare, UErrorCode& ec);

    /**
     * Insert the given integer into this vector at its sorted position
     * as defined by 'compare'.  The current elements are assumed to
     * be sorted already.
     */
    void sortedInsert(int32_t obj, UElementComparator *compare, UErrorCode& ec);

    /**
     * Sort the contents of the vector, assuming that the contents of the
     * vector are of type int32_t.
     */
    void sorti(UErrorCode &ec);

    /**
      * Sort the contents of this vector, using a caller-supplied function
      * to do the comparisons.  (It's confusing that
      *  UVector's UElementComparator function is different from the
      *  UComparator function type defined in uarrsort.h)
      */
    void sort(UElementComparator *compare, UErrorCode &ec);

    /**
     * Stable sort the contents of this vector using a caller-supplied function
     * of type UComparator to do the comparison.  Provides more flexibility
     * than UVector::sort() because an additional user parameter can be passed to
     * the comparison function.
     */
    void sortWithUComparator(UComparator *compare, const void *context, UErrorCode &ec);

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const;

private:
    void _init(int32_t initialCapacity, UErrorCode &status);

    int32_t indexOf(UElement key, int32_t startIndex = 0, int8_t hint = 0) const;

    void sortedInsert(UElement e, UElementComparator *compare, UErrorCode& ec);

    // Disallow
    UVector(const UVector&);

    // Disallow
    UVector& operator=(const UVector&);

};


/**
 * <p>Ultralightweight C++ implementation of a <tt>void*</tt> stack
 * that is (mostly) compatible with java.util.Stack.  As in java, this
 * is merely a paper thin layer around UVector.  See the UVector
 * documentation for further information.
 *
 * <p><b>Design notes</b>
 *
 * <p>The element at index <tt>n-1</tt> is (of course) the top of the
 * stack.
 *
 * <p>The poorly named <tt>empty()</tt> method doesn't empty the
 * stack; it determines if the stack is empty.
 *
 * @author Alan Liu
 */
class U_COMMON_API UStack : public UVector {
public:
    UStack(UErrorCode &status);

    UStack(int32_t initialCapacity, UErrorCode &status);

    UStack(UObjectDeleter *d, UElementsAreEqual *c, UErrorCode &status);

    UStack(UObjectDeleter *d, UElementsAreEqual *c, int32_t initialCapacity, UErrorCode &status);

    virtual ~UStack();

    // It's okay not to have a virtual destructor (in UVector)
    // because UStack has no special cleanup to do.

    UBool empty(void) const;

    void* peek(void) const;

    int32_t peeki(void) const;

    void* pop(void);

    int32_t popi(void);

    void* push(void* obj, UErrorCode &status);

    int32_t push(int32_t i, UErrorCode &status);

    /*
    If the object o occurs as an item in this stack,
    this method returns the 1-based distance from the top of the stack.
    */
    int32_t search(void* obj) const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const;

private:
    // Disallow
    UStack(const UStack&);

    // Disallow
    UStack& operator=(const UStack&);
};


// UVector inlines

inline int32_t UVector::size(void) const {
    return count;
}

inline UBool UVector::isEmpty(void) const {
    return count == 0;
}

inline UBool UVector::contains(void* obj) const {
    return indexOf(obj) >= 0;
}

inline UBool UVector::contains(int32_t obj) const {
    return indexOf(obj) >= 0;
}

inline void* UVector::firstElement(void) const {
    return elementAt(0);
}

inline void* UVector::lastElement(void) const {
    return elementAt(count-1);
}

inline int32_t UVector::lastElementi(void) const {
    return elementAti(count-1);
}

inline void* UVector::operator[](int32_t index) const {
    return elementAt(index);
}

inline UBool UVector::operator!=(const UVector& other) {
    return !operator==(other);
}

// UStack inlines

inline UBool UStack::empty(void) const {
    return isEmpty();
}

inline void* UStack::peek(void) const {
    return lastElement();
}

inline int32_t UStack::peeki(void) const {
    return lastElementi();
}

inline void* UStack::push(void* obj, UErrorCode &status) {
    addElement(obj, status);
    return obj;
}

inline int32_t UStack::push(int32_t i, UErrorCode &status) {
    addElement(i, status);
    return i;
}

U_NAMESPACE_END

#endif
