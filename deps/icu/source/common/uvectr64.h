// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1999-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

//
//  UVector64 is a class implementing a vector of 64 bit integers.
//            It is similar to UVector32, but holds int64_t values rather than int32_t.
//            Most of the code is unchanged from UVector.
//

#ifndef UVECTOR64_H
#define UVECTOR64_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "uhash.h"
#include "uassert.h"

U_NAMESPACE_BEGIN



/**
 * <p>Ultralightweight C++ implementation of an <tt>int64_t</tt> vector
 * that has a subset of methods from UVector32
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
 * <p><b>To do</b>
 *
 * <p>Improve the handling of index out of bounds errors.
 *
 */
class U_COMMON_API UVector64 : public UObject {
private:
    int32_t   count;

    int32_t   capacity;
    
    int32_t   maxCapacity;   // Limit beyond which capacity is not permitted to grow.

    int64_t*  elements;

public:
    UVector64(UErrorCode &status);

    UVector64(int32_t initialCapacity, UErrorCode &status);

    virtual ~UVector64();

    /**
     * Assign this object to another (make this a copy of 'other').
     * Use the 'assign' function to assign each element.
     */
    void assign(const UVector64& other, UErrorCode &ec);

    /**
     * Compare this vector with another.  They will be considered
     * equal if they are of the same size and all elements are equal,
     * as compared using this object's comparer.
     */
    UBool operator==(const UVector64& other);

    /**
     * Equivalent to !operator==()
     */
    inline UBool operator!=(const UVector64& other);

    //------------------------------------------------------------
    // subset of java.util.Vector API
    //------------------------------------------------------------

    void addElement(int64_t elem, UErrorCode &status);

    void setElementAt(int64_t elem, int32_t index);

    void insertElementAt(int64_t elem, int32_t index, UErrorCode &status);
    
    int64_t elementAti(int32_t index) const;

    //UBool equals(const UVector64 &other) const;

    int64_t lastElementi(void) const;

    //int32_t indexOf(int64_t elem, int32_t startIndex = 0) const;

    //UBool contains(int64_t elem) const;

    //UBool containsAll(const UVector64& other) const;

    //UBool removeAll(const UVector64& other);

    //UBool retainAll(const UVector64& other);

    //void removeElementAt(int32_t index);

    void removeAllElements();

    int32_t size(void) const;

    inline UBool isEmpty(void) const { return count == 0; }

    // Inline.  Use this one for speedy size check.
    inline UBool ensureCapacity(int32_t minimumCapacity, UErrorCode &status);

    // Out-of-line, handles actual growth.  Called by ensureCapacity() when necessary.
    UBool expandCapacity(int32_t minimumCapacity, UErrorCode &status);

    /**
     * Change the size of this vector as follows: If newSize is
     * smaller, then truncate the array, possibly deleting held
     * elements for i >= newSize.  If newSize is larger, grow the
     * array, filling in new slows with zero.
     */
    void setSize(int32_t newSize);

    //------------------------------------------------------------
    // New API
    //------------------------------------------------------------

    //UBool containsNone(const UVector64& other) const;


    //void sortedInsert(int64_t elem, UErrorCode& ec);

    /**
     * Returns a pointer to the internal array holding the vector.
     */
    int64_t *getBuffer() const;

    /**
     * Set the maximum allowed buffer capacity for this vector/stack.
     * Default with no limit set is unlimited, go until malloc() fails.
     * A Limit of zero means unlimited capacity.
     * Units are vector elements (64 bits each), not bytes.
     */
    void setMaxCapacity(int32_t limit);

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

    // Disallow
    UVector64(const UVector64&);

    // Disallow
    UVector64& operator=(const UVector64&);


    //  API Functions for Stack operations.
    //  In the original UVector, these were in a separate derived class, UStack.
    //  Here in UVector64, they are all together.
public:
    //UBool empty(void) const;   // TODO:  redundant, same as empty().  Remove it?

    //int64_t peeki(void) const;
    
    int64_t popi(void);
    
    int64_t push(int64_t i, UErrorCode &status);

    int64_t *reserveBlock(int32_t size, UErrorCode &status);
    int64_t *popFrame(int32_t size);
};


// UVector64 inlines

inline UBool UVector64::ensureCapacity(int32_t minimumCapacity, UErrorCode &status) {
    if ((minimumCapacity >= 0) && (capacity >= minimumCapacity)) {
        return TRUE;
    } else {
        return expandCapacity(minimumCapacity, status);
    }
}

inline int64_t UVector64::elementAti(int32_t index) const {
    return (0 <= index && index < count) ? elements[index] : 0;
}


inline void UVector64::addElement(int64_t elem, UErrorCode &status) {
    if (ensureCapacity(count + 1, status)) {
        elements[count] = elem;
        count++;
    }
}

inline int64_t *UVector64::reserveBlock(int32_t size, UErrorCode &status) {
    if (ensureCapacity(count+size, status) == FALSE) {
        return NULL;
    }
    int64_t  *rp = elements+count;
    count += size;
    return rp;
}

inline int64_t *UVector64::popFrame(int32_t size) {
    U_ASSERT(count >= size);
    count -= size;
    if (count < 0) {
        count = 0;
    }
    return elements+count-size;
}



inline int32_t UVector64::size(void) const {
    return count;
}

inline int64_t UVector64::lastElementi(void) const {
    return elementAti(count-1);
}

inline UBool UVector64::operator!=(const UVector64& other) {
    return !operator==(other);
}

inline int64_t *UVector64::getBuffer() const {
    return elements;
}


// UStack inlines

inline int64_t UVector64::push(int64_t i, UErrorCode &status) {
    addElement(i, status);
    return i;
}

inline int64_t UVector64::popi(void) {
    int64_t result = 0;
    if (count > 0) {
        count--;
        result = elements[count];
    }
    return result;
}

U_NAMESPACE_END

#endif
