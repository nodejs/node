// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2005-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  package.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2005aug25
*   created by: Markus W. Scherer
*
*   Read, modify, and write ICU .dat data package files.
*/

#ifndef __PACKAGE_H__
#define __PACKAGE_H__

#include "unicode/utypes.h"

#include <stdio.h>

// .dat package file representation ---------------------------------------- ***

#define STRING_STORE_SIZE 100000
#define MAX_PKG_NAME_LENGTH 64

typedef void CheckDependency(void *context, const char *itemName, const char *targetName);

U_NAMESPACE_BEGIN

struct Item {
    char *name;
    uint8_t *data;
    int32_t length;
    UBool isDataOwned;
    char type;
};

class U_TOOLUTIL_API Package {
public:
    /*
     * Constructor.
     * Prepare this object for a new, empty package.
     */
    Package();

    /* Destructor. */
    ~Package();

    /**
     * Uses the prefix of the first entry of the package in readPackage(),
     * rather than the package basename.
     */
    void setAutoPrefix() { doAutoPrefix=TRUE; }
    /**
     * Same as setAutoPrefix(), plus the prefix must end with the platform type letter.
     */
    void setAutoPrefixWithType() {
        doAutoPrefix=TRUE;
        prefixEndsWithType=TRUE;
    }
    void setPrefix(const char *p);

    /*
     * Read an existing .dat package file.
     * The header and item name strings are swapped into this object,
     * but the items are left unswapped.
     */
    void readPackage(const char *filename);
    /*
     * Write a .dat package file with the items in this object.
     * Swap all pieces to the desired output platform properties.
     * The package becomes unusable:
     * The item names are swapped and sorted in the outCharset rather than the local one.
     * Also, the items themselves are swapped in-place
     */
    void writePackage(const char *filename, char outType, const char *comment);

    /*
     * Return the input data type letter (l, b, or e).
     */
    char getInType();

    // find the item in items[], return the non-negative index if found, else the binary-not of the insertion point
    int32_t findItem(const char *name, int32_t length=-1) const;

    /*
     * Set internal state for following calls to findNextItem() which will return
     * indexes for items whose names match the pattern.
     */
    void findItems(const char *pattern);
    int32_t findNextItem();
    /*
     * Set the match mode for findItems() & findNextItem().
     * @param mode 0=default
     *             MATCH_NOSLASH * does not match a '/'
     */
    void setMatchMode(uint32_t mode);

    enum {
        MATCH_NOSLASH=1
    };

    void addItem(const char *name);
    void addItem(const char *name, uint8_t *data, int32_t length, UBool isDataOwned, char type);
    void addFile(const char *filesPath, const char *name);
    void addItems(const Package &listPkg);

    void removeItem(int32_t itemIndex);
    void removeItems(const char *pattern);
    void removeItems(const Package &listPkg);

    /* The extractItem() functions accept outputType=0 to mean "don't swap the item". */
    void extractItem(const char *filesPath, int32_t itemIndex, char outType);
    void extractItems(const char *filesPath, const char *pattern, char outType);
    void extractItems(const char *filesPath, const Package &listPkg, char outType);

    /* This variant extracts an item to a specific filename. */
    void extractItem(const char *filesPath, const char *outName, int32_t itemIndex, char outType);

    int32_t getItemCount() const;
    const Item *getItem(int32_t idx) const;

    /*
     * Check dependencies and return TRUE if all dependencies are fulfilled.
     */
    UBool checkDependencies();

    /*
     * Enumerate all the dependencies and give the results to context and call CheckDependency callback
     * @param context user context (will be passed to check function)
     * @param check will be called with context and any missing items
     */
    void enumDependencies(void *context, CheckDependency check);

private:
    void enumDependencies(Item *pItem, void *context, CheckDependency check);

    /**
     * Default CheckDependency function used by checkDependencies()
     */
    static void checkDependency(void *context, const char *itemName, const char *targetName);

    /*
     * Allocate a string in inStrings or outStrings.
     * The length does not include the terminating NUL.
     */
    char *allocString(UBool in, int32_t length);

    void sortItems();

    // data fields
    char inPkgName[MAX_PKG_NAME_LENGTH];
    char pkgPrefix[MAX_PKG_NAME_LENGTH];

    uint8_t *inData;
    uint8_t header[1024];
    int32_t inLength, headerLength;
    uint8_t inCharset;
    UBool inIsBigEndian;
    UBool doAutoPrefix;
    UBool prefixEndsWithType;

    int32_t itemCount;
    int32_t itemMax;
    Item   *items;

    int32_t inStringTop, outStringTop;
    char inStrings[STRING_STORE_SIZE], outStrings[STRING_STORE_SIZE];

    // match mode for findItems(pattern) and findNextItem()
    uint32_t matchMode;

    // state for findItems(pattern) and findNextItem()
    const char *findPrefix, *findSuffix;
    int32_t findPrefixLength, findSuffixLength;
    int32_t findNextIndex;

    // state for checkDependencies()
    UBool isMissingItems;

    /**
     * Grow itemMax to new value
     */
    void setItemCapacity(int32_t max);

    /**
     * Grow itemMax to at least itemCount+1
     */
    void ensureItemCapacity();
};

U_NAMESPACE_END

#endif


