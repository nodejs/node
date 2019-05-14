// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2000-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File reslist.h
*
* Modification History:
*
*   Date        Name        Description
*   02/21/00    weiv        Creation.
*******************************************************************************
*/

#ifndef RESLIST_H
#define RESLIST_H

#define KEY_SPACE_SIZE 65536
#define RESLIST_MAX_INT_VECTOR 2048

#include <functional>

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "cstring.h"
#include "uhash.h"
#include "unewdata.h"
#include "uresdata.h"
#include "ustr.h"

U_CDECL_BEGIN

class PathFilter;
class PseudoListResource;
class ResKeyPath;

struct ResFile {
    ResFile()
            : fBytes(NULL), fIndexes(NULL),
              fKeys(NULL), fKeysLength(0), fKeysCount(0),
              fStrings(NULL), fStringIndexLimit(0),
              fChecksum(0) {}
    ~ResFile() { close(); }

    void close();

    uint8_t *fBytes;
    const int32_t *fIndexes;
    const char *fKeys;
    int32_t fKeysLength;
    int32_t fKeysCount;

    PseudoListResource *fStrings;
    int32_t fStringIndexLimit;

    int32_t fChecksum;
};

struct SResource;

typedef struct KeyMapEntry {
    int32_t oldpos, newpos;
} KeyMapEntry;

/* Resource bundle root table */
struct SRBRoot {
    SRBRoot(const UString *comment, UBool isPoolBundle, UErrorCode &errorCode);
    ~SRBRoot();

    void write(const char *outputDir, const char *outputPkg,
               char *writtenFilename, int writtenFilenameLen, UErrorCode &errorCode);

    void setLocale(UChar *locale, UErrorCode &errorCode);
    int32_t addTag(const char *tag, UErrorCode &errorCode);

    const char *getKeyString(int32_t key) const;
    const char *getKeyBytes(int32_t *pLength) const;

    int32_t addKeyBytes(const char *keyBytes, int32_t length, UErrorCode &errorCode);

    void compactKeys(UErrorCode &errorCode);

    int32_t makeRes16(uint32_t resWord) const;
    int32_t mapKey(int32_t oldpos) const;

private:
    void compactStringsV2(UHashtable *stringSet, UErrorCode &errorCode);

public:
    // TODO: private

  SResource *fRoot;  // Normally a TableResource.
  char *fLocale;
  int32_t fIndexLength;
  int32_t fMaxTableLength;
  UBool fNoFallback; /* see URES_ATT_NO_FALLBACK */
  int8_t fStringsForm; /* default STRINGS_UTF16_V1 */
  UBool fIsPoolBundle;

  char *fKeys;
  KeyMapEntry *fKeyMap;
  int32_t fKeysBottom, fKeysTop;
  int32_t fKeysCapacity;
  int32_t fKeysCount;
  int32_t fLocalKeyLimit; /* key offset < limit fits into URES_TABLE */

  icu::UnicodeString f16BitUnits;
  int32_t f16BitStringsLength;

  const ResFile *fUsePoolBundle;
  int32_t fPoolStringIndexLimit;
  int32_t fPoolStringIndex16Limit;
  int32_t fLocalStringIndexLimit;
  SRBRoot *fWritePoolBundle;
};

/* write a java resource file */
// TODO: C++ify
void bundle_write_java(struct SRBRoot *bundle, const char *outputDir, const char* outputEnc, char *writtenFilename,
                       int writtenFilenameLen, const char* packageName, const char* bundleName, UErrorCode *status);

/* write a xml resource file */
// TODO: C++ify
void bundle_write_xml(struct SRBRoot *bundle, const char *outputDir,const char* outputEnc, const char* rbname,
                  char *writtenFilename, int writtenFilenameLen, const char* language, const char* package, UErrorCode *status);

/* Various resource types */

/*
 * Return a unique pointer to a dummy object,
 * for use in non-error cases when no resource is to be added to the bundle.
 * (NULL is used in error cases.)
 */
struct SResource* res_none(void);

class ArrayResource;
class TableResource;
class IntVectorResource;

TableResource *table_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status);

ArrayResource *array_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status);

struct SResource *string_open(struct SRBRoot *bundle, const char *tag, const UChar *value, int32_t len, const struct UString* comment, UErrorCode *status);

struct SResource *alias_open(struct SRBRoot *bundle, const char *tag, UChar *value, int32_t len, const struct UString* comment, UErrorCode *status);

IntVectorResource *intvector_open(struct SRBRoot *bundle, const char *tag,  const struct UString* comment, UErrorCode *status);

struct SResource *int_open(struct SRBRoot *bundle, const char *tag, int32_t value, const struct UString* comment, UErrorCode *status);

struct SResource *bin_open(struct SRBRoot *bundle, const char *tag, uint32_t length, uint8_t *data, const char* fileName, const struct UString* comment, UErrorCode *status);

/* Resource place holder */

struct SResource {
    SResource();
    SResource(SRBRoot *bundle, const char *tag, int8_t type, const UString* comment,
              UErrorCode &errorCode);
    virtual ~SResource();

    UBool isTable() const { return fType == URES_TABLE; }
    UBool isString() const { return fType == URES_STRING; }

    const char *getKeyString(const SRBRoot *bundle) const;

    /**
     * Preflights strings.
     * Finds duplicates and counts the total number of string code units
     * so that they can be written first to the 16-bit array,
     * for minimal string and container storage.
     *
     * We walk the final parse tree, rather than collecting this information while building it,
     * so that we need not deal with changes to the parse tree (especially removing resources).
     */
    void preflightStrings(SRBRoot *bundle, UHashtable *stringSet, UErrorCode &errorCode);
    virtual void handlePreflightStrings(SRBRoot *bundle, UHashtable *stringSet, UErrorCode &errorCode);

    /**
     * Writes resource values into f16BitUnits
     * and determines the resource item word, if possible.
     */
    void write16(SRBRoot *bundle);
    virtual void handleWrite16(SRBRoot *bundle);

    /**
     * Calculates ("preflights") and advances the *byteOffset
     * by the size of the resource's data in the binary file and
     * determines the resource item word.
     *
     * Most handlePreWrite() functions may add any number of bytes, but preWrite()
     * will always pad it to a multiple of 4.
     * The resource item type may be a related subtype of the fType.
     *
     * The preWrite() and write() functions start and end at the same
     * byteOffset values.
     * Prewriting allows bundle.write() to determine the root resource item word,
     * before actually writing the bundle contents to the file,
     * which is necessary because the root item is stored at the beginning.
     */
    void preWrite(uint32_t *byteOffset);
    virtual void handlePreWrite(uint32_t *byteOffset);

    /**
     * Writes the resource's data to mem and updates the byteOffset
     * in parallel.
     */
    void write(UNewDataMemory *mem, uint32_t *byteOffset);
    virtual void handleWrite(UNewDataMemory *mem, uint32_t *byteOffset);

    /**
     * Applies the given filter with the given base path to this resource.
     * Removes child resources rejected by the filter recursively.
     *
     * @param bundle Needed in order to access the key for this and child resources.
     */
    virtual void applyFilter(const PathFilter& filter, ResKeyPath& path, const SRBRoot* bundle);

    /**
     * Calls the given function for every key ID present in this tree.
     */
    virtual void collectKeys(std::function<void(int32_t)> collector) const;

    int8_t   fType;     /* nominal type: fRes (when != 0xffffffff) may use subtype */
    UBool    fWritten;  /* res_write() can exit early */
    uint32_t fRes;      /* resource item word; RES_BOGUS=0xffffffff if not known yet */
    int32_t  fRes16;    /* Res16 version of fRes for Table, Table16, Array16; -1 if it does not fit. */
    int32_t  fKey;      /* Index into bundle->fKeys; -1 if no key. */
    int32_t  fKey16;    /* Key16 version of fKey for Table & Table16; -1 if no key or it does not fit. */
    int      line;      /* used internally to report duplicate keys in tables */
    SResource *fNext;   /* This is for internal chaining while building */
    struct UString fComment;
};

class ContainerResource : public SResource {
public:
    ContainerResource(SRBRoot *bundle, const char *tag, int8_t type,
                      const UString* comment, UErrorCode &errorCode)
            : SResource(bundle, tag, type, comment, errorCode),
              fCount(0), fFirst(NULL) {}
    virtual ~ContainerResource();

    void handlePreflightStrings(SRBRoot *bundle, UHashtable *stringSet, UErrorCode &errorCode) override;

    void collectKeys(std::function<void(int32_t)> collector) const override;

protected:
    void writeAllRes16(SRBRoot *bundle);
    void preWriteAllRes(uint32_t *byteOffset);
    void writeAllRes(UNewDataMemory *mem, uint32_t *byteOffset);
    void writeAllRes32(UNewDataMemory *mem, uint32_t *byteOffset);

public:
    // TODO: private with getter?
    uint32_t fCount;
    SResource *fFirst;
};

class TableResource : public ContainerResource {
public:
    TableResource(SRBRoot *bundle, const char *tag,
                  const UString* comment, UErrorCode &errorCode)
            : ContainerResource(bundle, tag, URES_TABLE, comment, errorCode),
              fTableType(URES_TABLE), fRoot(bundle) {}
    virtual ~TableResource();

    void add(SResource *res, int linenumber, UErrorCode &errorCode);

    void handleWrite16(SRBRoot *bundle) override;
    void handlePreWrite(uint32_t *byteOffset) override;
    void handleWrite(UNewDataMemory *mem, uint32_t *byteOffset) override;

    void applyFilter(const PathFilter& filter, ResKeyPath& path, const SRBRoot* bundle) override;

    int8_t fTableType;  // determined by table_write16() for table_preWrite() & table_write()
    SRBRoot *fRoot;
};

class ArrayResource : public ContainerResource {
public:
    ArrayResource(SRBRoot *bundle, const char *tag,
                  const UString* comment, UErrorCode &errorCode)
            : ContainerResource(bundle, tag, URES_ARRAY, comment, errorCode),
              fLast(NULL) {}
    virtual ~ArrayResource();

    void add(SResource *res);

    virtual void handleWrite16(SRBRoot *bundle);
    virtual void handlePreWrite(uint32_t *byteOffset);
    virtual void handleWrite(UNewDataMemory *mem, uint32_t *byteOffset);

    SResource *fLast;
};

/**
 * List of resources for a pool bundle.
 * Writes an empty table resource, rather than a container structure.
 */
class PseudoListResource : public ContainerResource {
public:
    PseudoListResource(SRBRoot *bundle, UErrorCode &errorCode)
            : ContainerResource(bundle, NULL, URES_TABLE, NULL, errorCode) {}
    virtual ~PseudoListResource();

    void add(SResource *res);

    virtual void handleWrite16(SRBRoot *bundle);
};

class StringBaseResource : public SResource {
public:
    StringBaseResource(SRBRoot *bundle, const char *tag, int8_t type,
                       const UChar *value, int32_t len,
                       const UString* comment, UErrorCode &errorCode);
    StringBaseResource(SRBRoot *bundle, int8_t type,
                       const icu::UnicodeString &value, UErrorCode &errorCode);
    StringBaseResource(int8_t type, const UChar *value, int32_t len, UErrorCode &errorCode);
    virtual ~StringBaseResource();

    const UChar *getBuffer() const { return icu::toUCharPtr(fString.getBuffer()); }
    int32_t length() const { return fString.length(); }

    virtual void handlePreWrite(uint32_t *byteOffset);
    virtual void handleWrite(UNewDataMemory *mem, uint32_t *byteOffset);

    // TODO: private with getter?
    icu::UnicodeString fString;
};

class StringResource : public StringBaseResource {
public:
    StringResource(SRBRoot *bundle, const char *tag, const UChar *value, int32_t len,
                   const UString* comment, UErrorCode &errorCode)
            : StringBaseResource(bundle, tag, URES_STRING, value, len, comment, errorCode),
              fSame(NULL), fSuffixOffset(0),
              fNumCopies(0), fNumUnitsSaved(0), fNumCharsForLength(0) {}
    StringResource(SRBRoot *bundle, const icu::UnicodeString &value, UErrorCode &errorCode)
            : StringBaseResource(bundle, URES_STRING, value, errorCode),
              fSame(NULL), fSuffixOffset(0),
              fNumCopies(0), fNumUnitsSaved(0), fNumCharsForLength(0) {}
    StringResource(int32_t poolStringIndex, int8_t numCharsForLength,
                   const UChar *value, int32_t length,
                   UErrorCode &errorCode)
            : StringBaseResource(URES_STRING, value, length, errorCode),
              fSame(NULL), fSuffixOffset(0),
              fNumCopies(0), fNumUnitsSaved(0), fNumCharsForLength(numCharsForLength) {
        // v3 pool string encoded as string-v2 with low offset
        fRes = URES_MAKE_RESOURCE(URES_STRING_V2, poolStringIndex);
        fWritten = TRUE;
    }
    virtual ~StringResource();

    int32_t get16BitStringsLength() const {
        return fNumCharsForLength + length() + 1;  // +1 for the NUL
    }

    virtual void handlePreflightStrings(SRBRoot *bundle, UHashtable *stringSet, UErrorCode &errorCode);
    virtual void handleWrite16(SRBRoot *bundle);

    void writeUTF16v2(int32_t base, icu::UnicodeString &dest);

    StringResource *fSame;  // used for duplicates
    int32_t fSuffixOffset;  // this string is a suffix of fSame at this offset
    int32_t fNumCopies;     // number of equal strings represented by one stringSet element
    int32_t fNumUnitsSaved;  // from not writing duplicates and suffixes
    int8_t fNumCharsForLength;
};

class AliasResource : public StringBaseResource {
public:
    AliasResource(SRBRoot *bundle, const char *tag, const UChar *value, int32_t len,
                  const UString* comment, UErrorCode &errorCode)
            : StringBaseResource(bundle, tag, URES_ALIAS, value, len, comment, errorCode) {}
    virtual ~AliasResource();
};

class IntResource : public SResource {
public:
    IntResource(SRBRoot *bundle, const char *tag, int32_t value,
                const UString* comment, UErrorCode &errorCode);
    virtual ~IntResource();

    // TODO: private with getter?
    int32_t fValue;
};

class IntVectorResource : public SResource {
public:
    IntVectorResource(SRBRoot *bundle, const char *tag,
                      const UString* comment, UErrorCode &errorCode);
    virtual ~IntVectorResource();

    void add(int32_t value, UErrorCode &errorCode);

    virtual void handlePreWrite(uint32_t *byteOffset);
    virtual void handleWrite(UNewDataMemory *mem, uint32_t *byteOffset);

    // TODO: UVector32
    uint32_t fCount;
    uint32_t *fArray;
};

class BinaryResource : public SResource {
public:
    BinaryResource(SRBRoot *bundle, const char *tag,
                   uint32_t length, uint8_t *data, const char* fileName,
                   const UString* comment, UErrorCode &errorCode);
    virtual ~BinaryResource();

    virtual void handlePreWrite(uint32_t *byteOffset);
    virtual void handleWrite(UNewDataMemory *mem, uint32_t *byteOffset);

    // TODO: CharString?
    uint32_t fLength;
    uint8_t *fData;
    // TODO: CharString
    char* fFileName;  // file name for binary or import binary tags if any
};

// TODO: use LocalPointer or delete
void res_close(struct SResource *res);

void setIncludeCopyright(UBool val);
UBool getIncludeCopyright(void);

void setFormatVersion(int32_t formatVersion);

int32_t getFormatVersion();

void setUsePoolBundle(UBool use);

/* in wrtxml.cpp */
uint32_t computeCRC(const char *ptr, uint32_t len, uint32_t lastcrc);

U_CDECL_END
#endif /* #ifndef RESLIST_H */
