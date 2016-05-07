/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
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

U_NAMESPACE_BEGIN

class ResourceTableSink;

// Note: In C++, we use const char * pointers for keys,
// rather than an abstraction like Java UResource.Key.

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
        return UnicodeString(TRUE, r, len);
    }

    /**
     * Sets U_RESOURCE_TYPE_MISMATCH if this is not an alias resource.
     */
    virtual const UChar *getAliasString(int32_t &length, UErrorCode &errorCode) const = 0;

    inline UnicodeString getAliasUnicodeString(UErrorCode &errorCode) const {
        int32_t len = 0;
        const UChar *r = getAliasString(len, errorCode);
        return UnicodeString(TRUE, r, len);
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

protected:
    ResourceValue() {}

private:
    ResourceValue(const ResourceValue &);  // no copy constructor
    ResourceValue &operator=(const ResourceValue &);  // no assignment operator
};

/**
 * Sink for ICU resource array contents.
 * The base class does nothing.
 *
 * Nested arrays and tables are stored as nested sinks,
 * never put() as ResourceValue items.
 */
class U_COMMON_API ResourceArraySink : public UObject {
public:
    ResourceArraySink() {}
    virtual ~ResourceArraySink();

    /**
     * Adds a value from a resource array.
     *
     * @param index of the resource array item
     * @param value resource value
     */
    virtual void put(int32_t index, const ResourceValue &value, UErrorCode &errorCode);

    /**
     * Returns a nested resource array at the array index as another sink.
     * Creates the sink if none exists for the key.
     * Returns NULL if nested arrays are not supported.
     * The default implementation always returns NULL.
     *
     * This sink (not the caller) owns the nested sink.
     *
     * @param index of the resource array item
     * @param size number of array items
     * @return nested-array sink, or NULL
     */
    virtual ResourceArraySink *getOrCreateArraySink(
            int32_t index, int32_t size, UErrorCode &errorCode);

    /**
     * Returns a nested resource table at the array index as another sink.
     * Creates the sink if none exists for the key.
     * Returns NULL if nested tables are not supported.
     * The default implementation always returns NULL.
     *
     * This sink (not the caller) owns the nested sink.
     *
     * @param index of the resource array item
     * @param initialSize size hint for creating the sink if necessary
     * @return nested-table sink, or NULL
     */
    virtual ResourceTableSink *getOrCreateTableSink(
            int32_t index, int32_t initialSize, UErrorCode &errorCode);

    /**
     * "Leaves" the array.
     * Indicates that all of the resources and sub-resources of the current array
     * have been enumerated.
     */
    virtual void leave(UErrorCode &errorCode);

private:
    ResourceArraySink(const ResourceArraySink &);  // no copy constructor
    ResourceArraySink &operator=(const ResourceArraySink &);  // no assignment operator
};

/**
 * Sink for ICU resource table contents.
 * The base class does nothing.
 *
 * Nested arrays and tables are stored as nested sinks,
 * never put() as ResourceValue items.
 */
class U_COMMON_API ResourceTableSink : public UObject {
public:
    ResourceTableSink() {}
    virtual ~ResourceTableSink();

    /**
     * Adds a key-value pair from a resource table.
     *
     * @param key resource key string
     * @param value resource value
     */
    virtual void put(const char *key, const ResourceValue &value, UErrorCode &errorCode);

    /**
     * Adds a no-fallback/no-inheritance marker for this key.
     * Used for CLDR no-fallback data values of (three empty-set symbols)=={2205, 2205, 2205}
     * when enumerating tables with fallback from the specific resource bundle to root.
     *
     * The default implementation does nothing.
     *
     * @param key to be removed
     */
    virtual void putNoFallback(const char *key, UErrorCode &errorCode);

    /**
     * Returns a nested resource array for the key as another sink.
     * Creates the sink if none exists for the key.
     * Returns NULL if nested arrays are not supported.
     * The default implementation always returns NULL.
     *
     * This sink (not the caller) owns the nested sink.
     *
     * @param key resource key string
     * @param size number of array items
     * @return nested-array sink, or NULL
     */
    virtual ResourceArraySink *getOrCreateArraySink(
            const char *key, int32_t size, UErrorCode &errorCode);

    /**
     * Returns a nested resource table for the key as another sink.
     * Creates the sink if none exists for the key.
     * Returns NULL if nested tables are not supported.
     * The default implementation always returns NULL.
     *
     * This sink (not the caller) owns the nested sink.
     *
     * @param key resource key string
     * @param initialSize size hint for creating the sink if necessary
     * @return nested-table sink, or NULL
     */
    virtual ResourceTableSink *getOrCreateTableSink(
            const char *key, int32_t initialSize, UErrorCode &errorCode);

    /**
     * "Leaves" the table.
     * Indicates that all of the resources and sub-resources of the current table
     * have been enumerated.
     */
    virtual void leave(UErrorCode &errorCode);

private:
    ResourceTableSink(const ResourceTableSink &);  // no copy constructor
    ResourceTableSink &operator=(const ResourceTableSink &);  // no assignment operator
};

U_NAMESPACE_END

#endif
