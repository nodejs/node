// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* resource.h
*
* created on: 2015nov04
* created by: Markus W. Scherer
*/

#ifndef __URESOURCE_H__
#define __URESOURCE_H__

/**
 * \file
 * \brief ICU resource bundle key and value types.
 */

// Note: Ported from ICU4J class UResource and its nested classes,
// but the C++ classes are separate, not nested.

// We use the Resource prefix for C++ classes, as usual.
// The UResource prefix would be used for C types.

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/ures.h"
#include "restrace.h"

struct ResourceData;

U_NAMESPACE_BEGIN

class ResourceValue;

// Note: In C++, we use const char * pointers for keys,
// rather than an abstraction like Java UResource.Key.

/**
 * Interface for iterating over a resource bundle array resource.
 */
class U_COMMON_API ResourceArray {
public:
    /** Constructs an empty array object. */
    ResourceArray() : items16(NULL), items32(NULL), length(0) {}

    /** Only for implementation use. @internal */
    ResourceArray(const uint16_t *i16, const uint32_t *i32, int32_t len,
                  const ResourceTracer& traceInfo) :
            items16(i16), items32(i32), length(len),
            fTraceInfo(traceInfo) {}

    /**
     * @return The number of items in the array resource.
     */
    int32_t getSize() const { return length; }
    /**
     * @param i Array item index.
     * @param value Output-only, receives the value of the i'th item.
     * @return true if i is non-negative and less than getSize().
     */
    UBool getValue(int32_t i, ResourceValue &value) const;

    /** Only for implementation use. @internal */
    uint32_t internalGetResource(const ResourceData *pResData, int32_t i) const;

private:
    const uint16_t *items16;
    const uint32_t *items32;
    int32_t length;
    ResourceTracer fTraceInfo;
};

/**
 * Interface for iterating over a resource bundle table resource.
 */
class U_COMMON_API ResourceTable {
public:
    /** Constructs an empty table object. */
    ResourceTable() : keys16(NULL), keys32(NULL), items16(NULL), items32(NULL), length(0) {}

    /** Only for implementation use. @internal */
    ResourceTable(const uint16_t *k16, const int32_t *k32,
                  const uint16_t *i16, const uint32_t *i32, int32_t len,
                  const ResourceTracer& traceInfo) :
            keys16(k16), keys32(k32), items16(i16), items32(i32), length(len),
            fTraceInfo(traceInfo) {}

    /**
     * @return The number of items in the array resource.
     */
    int32_t getSize() const { return length; }
    /**
     * @param i Table item index.
     * @param key Output-only, receives the key of the i'th item.
     * @param value Output-only, receives the value of the i'th item.
     * @return true if i is non-negative and less than getSize().
     */
    UBool getKeyAndValue(int32_t i, const char *&key, ResourceValue &value) const;

    /**
     * @param key Key string to find in the table.
     * @param value Output-only, receives the value of the item with that key.
     * @return true if the table contains the key.
     */
    UBool findValue(const char *key, ResourceValue &value) const;

private:
    const uint16_t *keys16;
    const int32_t *keys32;
    const uint16_t *items16;
    const uint32_t *items32;
    int32_t length;
    ResourceTracer fTraceInfo;
};

/**
 * Represents a resource bundle item's value.
 * Avoids object creations as much as possible.
 * Mutable, not thread-safe.
 */
class U_COMMON_API ResourceValue : public UObject {
public:
    virtual ~ResourceValue();

    /**
     * @return ICU resource type, for example, URES_STRING
     */
    virtual UResType getType() const = 0;

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not a string resource.
     *
     * @see ures_getString()
     */
    virtual const UChar *getString(int32_t &length, UErrorCode &errorCode) const = 0;

    inline UnicodeString getUnicodeString(UErrorCode &errorCode) const {
        int32_t len = 0;
        const UChar *r = getString(len, errorCode);
        return UnicodeString(true, r, len);
    }

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not an alias resource.
     */
    virtual const UChar *getAliasString(int32_t &length, UErrorCode &errorCode) const = 0;

    inline UnicodeString getAliasUnicodeString(UErrorCode &errorCode) const {
        int32_t len = 0;
        const UChar *r = getAliasString(len, errorCode);
        return UnicodeString(true, r, len);
    }

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not an integer resource.
     *
     * @see ures_getInt()
     */
    virtual int32_t getInt(UErrorCode &errorCode) const = 0;

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not an integer resource.
     *
     * @see ures_getUInt()
     */
    virtual uint32_t getUInt(UErrorCode &errorCode) const = 0;

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not an intvector resource.
     *
     * @see ures_getIntVector()
     */
    virtual const int32_t *getIntVector(int32_t &length, UErrorCode &errorCode) const = 0;

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not a binary-blob resource.
     *
     * @see ures_getBinary()
     */
    virtual const uint8_t *getBinary(int32_t &length, UErrorCode &errorCode) const = 0;

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not an array resource
     */
    virtual ResourceArray getArray(UErrorCode &errorCode) const = 0;

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not a table resource
     */
    virtual ResourceTable getTable(UErrorCode &errorCode) const = 0;

    /**
     * Is this a no-fallback/no-inheritance marker string?
     * Such a marker is used for
     * CLDR no-fallback data values of (three empty-set symbols)=={2205, 2205, 2205}
     * when enumerating tables with fallback from the specific resource bundle to root.
     *
     * @return true if this is a no-inheritance marker string
     */
    virtual UBool isNoInheritanceMarker() const = 0;

    /**
     * Sets the dest strings from the string values in this array resource.
     *
     * @return the number of strings in this array resource.
     *     If greater than capacity, then an overflow error is set.
     *
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not an array resource
     *     or if any of the array items is not a string
     */
    virtual int32_t getStringArray(UnicodeString *dest, int32_t capacity,
                                   UErrorCode &errorCode) const = 0;

    /**
     * Same as
     * <pre>
     * if (getType() == URES_STRING) {
     *     return new String[] { getString(); }
     * } else {
     *     return getStringArray();
     * }
     * </pre>
     *
     * Sets U_RESOURCE_TYPE_MISMATCH if this is
     *     neither a string resource nor an array resource containing strings
     * @see getString()
     * @see getStringArray()
     */
    virtual int32_t getStringArrayOrStringAsArray(UnicodeString *dest, int32_t capacity,
                                                  UErrorCode &errorCode) const = 0;

    /**
     * Same as
     * <pre>
     * if (getType() == URES_STRING) {
     *     return getString();
     * } else {
     *     return getStringArray()[0];
     * }
     * </pre>
     *
     * Sets U_RESOURCE_TYPE_MISMATCH if this is
     *     neither a string resource nor an array resource containing strings
     * @see getString()
     * @see getStringArray()
     */
    virtual UnicodeString getStringOrFirstOfArray(UErrorCode &errorCode) const = 0;

protected:
    ResourceValue() {}

private:
    ResourceValue(const ResourceValue &);  // no copy constructor
    ResourceValue &operator=(const ResourceValue &);  // no assignment operator
};

/**
 * Sink for ICU resource bundle contents.
 */
class U_COMMON_API ResourceSink : public UObject {
public:
    ResourceSink() {}
    virtual ~ResourceSink();

    /**
     * Called once for each bundle (child-parent-...-root).
     * The value is normally an array or table resource,
     * and implementations of this method normally iterate over the
     * tree of resource items stored there.
     *
     * @param key The key string of the enumeration-start resource.
     *     Empty if the enumeration starts at the top level of the bundle.
     * @param value Call getArray() or getTable() as appropriate. Then reuse for
     *     output values from Array and Table getters. Note: ResourceTable and
     *     ResourceArray instances must outlive the ResourceValue instance for
     *     ResourceTracer to be happy.
     * @param noFallback true if the bundle has no parent;
     *     that is, its top-level table has the nofallback attribute,
     *     or it is the root bundle of a locale tree.
     */
    virtual void put(const char *key, ResourceValue &value, UBool noFallback,
                     UErrorCode &errorCode) = 0;

private:
    ResourceSink(const ResourceSink &);  // no copy constructor
    ResourceSink &operator=(const ResourceSink &);  // no assignment operator
};

U_NAMESPACE_END

#endif
