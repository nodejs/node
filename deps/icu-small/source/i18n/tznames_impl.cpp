// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2011-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File TZNAMES_IMPL.CPP
*
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/ustring.h"
#include "unicode/timezone.h"

#include "tznames_impl.h"
#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"
#include "mutex.h"
#include "resource.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "zonemeta.h"
#include "ucln_in.h"
#include "uvector.h"
#include "olsontz.h"

U_NAMESPACE_BEGIN

#define ZID_KEY_MAX  128
#define MZ_PREFIX_LEN 5

static const char gZoneStrings[]        = "zoneStrings";
static const char gMZPrefix[]           = "meta:";

static const char EMPTY[]               = "<empty>";   // place holder for empty ZNames
static const char DUMMY_LOADER[]        = "<dummy>";   // place holder for dummy ZNamesLoader
static const UChar NO_NAME[]            = { 0 };   // for empty no-fallback time zone names

// stuff for TZDBTimeZoneNames
static const char* TZDBNAMES_KEYS[]               = {"ss", "sd"};
static const int32_t TZDBNAMES_KEYS_SIZE = UPRV_LENGTHOF(TZDBNAMES_KEYS);

static UMutex gTZDBNamesMapLock = U_MUTEX_INITIALIZER;
static UMutex gDataMutex = U_MUTEX_INITIALIZER;

static UHashtable* gTZDBNamesMap = NULL;
static icu::UInitOnce gTZDBNamesMapInitOnce = U_INITONCE_INITIALIZER;

static TextTrieMap* gTZDBNamesTrie = NULL;
static icu::UInitOnce gTZDBNamesTrieInitOnce = U_INITONCE_INITIALIZER;

// The order in which strings are stored may be different than the order in the public enum.
enum UTimeZoneNameTypeIndex {
    UTZNM_INDEX_UNKNOWN = -1,
    UTZNM_INDEX_EXEMPLAR_LOCATION,
    UTZNM_INDEX_LONG_GENERIC,
    UTZNM_INDEX_LONG_STANDARD,
    UTZNM_INDEX_LONG_DAYLIGHT,
    UTZNM_INDEX_SHORT_GENERIC,
    UTZNM_INDEX_SHORT_STANDARD,
    UTZNM_INDEX_SHORT_DAYLIGHT,
    UTZNM_INDEX_COUNT
};
static const UChar* EMPTY_NAMES[UTZNM_INDEX_COUNT] = {0,0,0,0,0,0,0};

U_CDECL_BEGIN
static UBool U_CALLCONV tzdbTimeZoneNames_cleanup(void) {
    if (gTZDBNamesMap != NULL) {
        uhash_close(gTZDBNamesMap);
        gTZDBNamesMap = NULL;
    }
    gTZDBNamesMapInitOnce.reset();

    if (gTZDBNamesTrie != NULL) {
        delete gTZDBNamesTrie;
        gTZDBNamesTrie = NULL;
    }
    gTZDBNamesTrieInitOnce.reset();

    return TRUE;
}
U_CDECL_END

/**
 * ZNameInfo stores zone name information in the trie
 */
struct ZNameInfo {
    UTimeZoneNameType   type;
    const UChar*        tzID;
    const UChar*        mzID;
};

/**
 * ZMatchInfo stores zone name match information used by find method
 */
struct ZMatchInfo {
    const ZNameInfo*    znameInfo;
    int32_t             matchLength;
};

// Helper functions
static void mergeTimeZoneKey(const UnicodeString& mzID, char* result);

#define DEFAULT_CHARACTERNODE_CAPACITY 1

// ---------------------------------------------------
// CharacterNode class implementation
// ---------------------------------------------------
void CharacterNode::clear() {
    uprv_memset(this, 0, sizeof(*this));
}

void CharacterNode::deleteValues(UObjectDeleter *valueDeleter) {
    if (fValues == NULL) {
        // Do nothing.
    } else if (!fHasValuesVector) {
        if (valueDeleter) {
            valueDeleter(fValues);
        }
    } else {
        delete (UVector *)fValues;
    }
}

void
CharacterNode::addValue(void *value, UObjectDeleter *valueDeleter, UErrorCode &status) {
    if (U_FAILURE(status)) {
        if (valueDeleter) {
            valueDeleter(value);
        }
        return;
    }
    if (fValues == NULL) {
        fValues = value;
    } else {
        // At least one value already.
        if (!fHasValuesVector) {
            // There is only one value so far, and not in a vector yet.
            // Create a vector and add the old value.
            UVector *values = new UVector(valueDeleter, NULL, DEFAULT_CHARACTERNODE_CAPACITY, status);
            if (U_FAILURE(status)) {
                if (valueDeleter) {
                    valueDeleter(value);
                }
                return;
            }
            values->addElement(fValues, status);
            fValues = values;
            fHasValuesVector = TRUE;
        }
        // Add the new value.
        ((UVector *)fValues)->addElement(value, status);
    }
}

// ---------------------------------------------------
// TextTrieMapSearchResultHandler class implementation
// ---------------------------------------------------
TextTrieMapSearchResultHandler::~TextTrieMapSearchResultHandler(){
}

// ---------------------------------------------------
// TextTrieMap class implementation
// ---------------------------------------------------
TextTrieMap::TextTrieMap(UBool ignoreCase, UObjectDeleter *valueDeleter)
: fIgnoreCase(ignoreCase), fNodes(NULL), fNodesCapacity(0), fNodesCount(0),
  fLazyContents(NULL), fIsEmpty(TRUE), fValueDeleter(valueDeleter) {
}

TextTrieMap::~TextTrieMap() {
    int32_t index;
    for (index = 0; index < fNodesCount; ++index) {
        fNodes[index].deleteValues(fValueDeleter);
    }
    uprv_free(fNodes);
    if (fLazyContents != NULL) {
        for (int32_t i=0; i<fLazyContents->size(); i+=2) {
            if (fValueDeleter) {
                fValueDeleter(fLazyContents->elementAt(i+1));
            }
        }
        delete fLazyContents;
    }
}

int32_t TextTrieMap::isEmpty() const {
    // Use a separate field for fIsEmpty because it will remain unchanged once the
    //   Trie is built, while fNodes and fLazyContents change with the lazy init
    //   of the nodes structure.  Trying to test the changing fields has
    //   thread safety complications.
    return fIsEmpty;
}


//  We defer actually building the TextTrieMap node structure until the first time a
//     search is performed.  put() simply saves the parameters in case we do
//     eventually need to build it.
//
void
TextTrieMap::put(const UnicodeString &key, void *value, ZNStringPool &sp, UErrorCode &status) {
    const UChar *s = sp.get(key, status);
    put(s, value, status);
}

// This method is designed for a persistent key, such as string key stored in
// resource bundle.
void
TextTrieMap::put(const UChar *key, void *value, UErrorCode &status) {
    fIsEmpty = FALSE;
    if (fLazyContents == NULL) {
        fLazyContents = new UVector(status);
        if (fLazyContents == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    if (U_FAILURE(status)) {
        if (fValueDeleter) {
            fValueDeleter((void*) key);
        }
        return;
    }
    U_ASSERT(fLazyContents != NULL);

    UChar *s = const_cast<UChar *>(key);
    fLazyContents->addElement(s, status);
    if (U_FAILURE(status)) {
        if (fValueDeleter) {
            fValueDeleter((void*) key);
        }
        return;
    }

    fLazyContents->addElement(value, status);
}

void
TextTrieMap::putImpl(const UnicodeString &key, void *value, UErrorCode &status) {
    if (fNodes == NULL) {
        fNodesCapacity = 512;
        fNodes = (CharacterNode *)uprv_malloc(fNodesCapacity * sizeof(CharacterNode));
        if (fNodes == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        fNodes[0].clear();  // Init root node.
        fNodesCount = 1;
    }

    UnicodeString foldedKey;
    const UChar *keyBuffer;
    int32_t keyLength;
    if (fIgnoreCase) {
        // Ok to use fastCopyFrom() because we discard the copy when we return.
        foldedKey.fastCopyFrom(key).foldCase();
        keyBuffer = foldedKey.getBuffer();
        keyLength = foldedKey.length();
    } else {
        keyBuffer = key.getBuffer();
        keyLength = key.length();
    }

    CharacterNode *node = fNodes;
    int32_t index;
    for (index = 0; index < keyLength; ++index) {
        node = addChildNode(node, keyBuffer[index], status);
    }
    node->addValue(value, fValueDeleter, status);
}

UBool
TextTrieMap::growNodes() {
    if (fNodesCapacity == 0xffff) {
        return FALSE;  // We use 16-bit node indexes.
    }
    int32_t newCapacity = fNodesCapacity + 1000;
    if (newCapacity > 0xffff) {
        newCapacity = 0xffff;
    }
    CharacterNode *newNodes = (CharacterNode *)uprv_malloc(newCapacity * sizeof(CharacterNode));
    if (newNodes == NULL) {
        return FALSE;
    }
    uprv_memcpy(newNodes, fNodes, fNodesCount * sizeof(CharacterNode));
    uprv_free(fNodes);
    fNodes = newNodes;
    fNodesCapacity = newCapacity;
    return TRUE;
}

CharacterNode*
TextTrieMap::addChildNode(CharacterNode *parent, UChar c, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    // Linear search of the sorted list of children.
    uint16_t prevIndex = 0;
    uint16_t nodeIndex = parent->fFirstChild;
    while (nodeIndex > 0) {
        CharacterNode *current = fNodes + nodeIndex;
        UChar childCharacter = current->fCharacter;
        if (childCharacter == c) {
            return current;
        } else if (childCharacter > c) {
            break;
        }
        prevIndex = nodeIndex;
        nodeIndex = current->fNextSibling;
    }

    // Ensure capacity. Grow fNodes[] if needed.
    if (fNodesCount == fNodesCapacity) {
        int32_t parentIndex = (int32_t)(parent - fNodes);
        if (!growNodes()) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        parent = fNodes + parentIndex;
    }

    // Insert a new child node with c in sorted order.
    CharacterNode *node = fNodes + fNodesCount;
    node->clear();
    node->fCharacter = c;
    node->fNextSibling = nodeIndex;
    if (prevIndex == 0) {
        parent->fFirstChild = (uint16_t)fNodesCount;
    } else {
        fNodes[prevIndex].fNextSibling = (uint16_t)fNodesCount;
    }
    ++fNodesCount;
    return node;
}

CharacterNode*
TextTrieMap::getChildNode(CharacterNode *parent, UChar c) const {
    // Linear search of the sorted list of children.
    uint16_t nodeIndex = parent->fFirstChild;
    while (nodeIndex > 0) {
        CharacterNode *current = fNodes + nodeIndex;
        UChar childCharacter = current->fCharacter;
        if (childCharacter == c) {
            return current;
        } else if (childCharacter > c) {
            break;
        }
        nodeIndex = current->fNextSibling;
    }
    return NULL;
}

// Mutex for protecting the lazy creation of the Trie node structure on the first call to search().
static UMutex TextTrieMutex = U_MUTEX_INITIALIZER;

// buildTrie() - The Trie node structure is needed.  Create it from the data that was
//               saved at the time the ZoneStringFormatter was created.  The Trie is only
//               needed for parsing operations, which are less common than formatting,
//               and the Trie is big, which is why its creation is deferred until first use.
void TextTrieMap::buildTrie(UErrorCode &status) {
    if (fLazyContents != NULL) {
        for (int32_t i=0; i<fLazyContents->size(); i+=2) {
            const UChar *key = (UChar *)fLazyContents->elementAt(i);
            void  *val = fLazyContents->elementAt(i+1);
            UnicodeString keyString(TRUE, key, -1);  // Aliasing UnicodeString constructor.
            putImpl(keyString, val, status);
        }
        delete fLazyContents;
        fLazyContents = NULL;
    }
}

void
TextTrieMap::search(const UnicodeString &text, int32_t start,
                  TextTrieMapSearchResultHandler *handler, UErrorCode &status) const {
    {
        // TODO: if locking the mutex for each check proves to be a performance problem,
        //       add a flag of type atomic_int32_t to class TextTrieMap, and use only
        //       the ICU atomic safe functions for assigning and testing.
        //       Don't test the pointer fLazyContents.
        //       Don't do unless it's really required.
        Mutex lock(&TextTrieMutex);
        if (fLazyContents != NULL) {
            TextTrieMap *nonConstThis = const_cast<TextTrieMap *>(this);
            nonConstThis->buildTrie(status);
        }
    }
    if (fNodes == NULL) {
        return;
    }
    search(fNodes, text, start, start, handler, status);
}

void
TextTrieMap::search(CharacterNode *node, const UnicodeString &text, int32_t start,
                  int32_t index, TextTrieMapSearchResultHandler *handler, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return;
    }
    if (node->hasValues()) {
        if (!handler->handleMatch(index - start, node, status)) {
            return;
        }
        if (U_FAILURE(status)) {
            return;
        }
    }
    UChar32 c = text.char32At(index);
    if (fIgnoreCase) {
        // size of character may grow after fold operation
        UnicodeString tmp(c);
        tmp.foldCase();
        int32_t tmpidx = 0;
        while (tmpidx < tmp.length()) {
            c = tmp.char32At(tmpidx);
            node = getChildNode(node, c);
            if (node == NULL) {
                break;
            }
            tmpidx = tmp.moveIndex32(tmpidx, 1);
        }
    } else {
        node = getChildNode(node, c);
    }
    if (node != NULL) {
        search(node, text, start, index+1, handler, status);
    }
}

// ---------------------------------------------------
// ZNStringPool class implementation
// ---------------------------------------------------
static const int32_t POOL_CHUNK_SIZE = 2000;
struct ZNStringPoolChunk: public UMemory {
    ZNStringPoolChunk    *fNext;                       // Ptr to next pool chunk
    int32_t               fLimit;                       // Index to start of unused area at end of fStrings
    UChar                 fStrings[POOL_CHUNK_SIZE];    //  Strings array
    ZNStringPoolChunk();
};

ZNStringPoolChunk::ZNStringPoolChunk() {
    fNext = NULL;
    fLimit = 0;
}

ZNStringPool::ZNStringPool(UErrorCode &status) {
    fChunks = NULL;
    fHash   = NULL;
    if (U_FAILURE(status)) {
        return;
    }
    fChunks = new ZNStringPoolChunk;
    if (fChunks == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    fHash   = uhash_open(uhash_hashUChars      /* keyHash */,
                         uhash_compareUChars   /* keyComp */,
                         uhash_compareUChars   /* valueComp */,
                         &status);
    if (U_FAILURE(status)) {
        return;
    }
}

ZNStringPool::~ZNStringPool() {
    if (fHash != NULL) {
        uhash_close(fHash);
        fHash = NULL;
    }

    while (fChunks != NULL) {
        ZNStringPoolChunk *nextChunk = fChunks->fNext;
        delete fChunks;
        fChunks = nextChunk;
    }
}

static const UChar EmptyString = 0;

const UChar *ZNStringPool::get(const UChar *s, UErrorCode &status) {
    const UChar *pooledString;
    if (U_FAILURE(status)) {
        return &EmptyString;
    }

    pooledString = static_cast<UChar *>(uhash_get(fHash, s));
    if (pooledString != NULL) {
        return pooledString;
    }

    int32_t length = u_strlen(s);
    int32_t remainingLength = POOL_CHUNK_SIZE - fChunks->fLimit;
    if (remainingLength <= length) {
        U_ASSERT(length < POOL_CHUNK_SIZE);
        if (length >= POOL_CHUNK_SIZE) {
            status = U_INTERNAL_PROGRAM_ERROR;
            return &EmptyString;
        }
        ZNStringPoolChunk *oldChunk = fChunks;
        fChunks = new ZNStringPoolChunk;
        if (fChunks == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return &EmptyString;
        }
        fChunks->fNext = oldChunk;
    }

    UChar *destString = &fChunks->fStrings[fChunks->fLimit];
    u_strcpy(destString, s);
    fChunks->fLimit += (length + 1);
    uhash_put(fHash, destString, destString, &status);
    return destString;
}


//
//  ZNStringPool::adopt()    Put a string into the hash, but do not copy the string data
//                           into the pool's storage.  Used for strings from resource bundles,
//                           which will perisist for the life of the zone string formatter, and
//                           therefore can be used directly without copying.
const UChar *ZNStringPool::adopt(const UChar * s, UErrorCode &status) {
    const UChar *pooledString;
    if (U_FAILURE(status)) {
        return &EmptyString;
    }
    if (s != NULL) {
        pooledString = static_cast<UChar *>(uhash_get(fHash, s));
        if (pooledString == NULL) {
            UChar *ncs = const_cast<UChar *>(s);
            uhash_put(fHash, ncs, ncs, &status);
        }
    }
    return s;
}


const UChar *ZNStringPool::get(const UnicodeString &s, UErrorCode &status) {
    UnicodeString &nonConstStr = const_cast<UnicodeString &>(s);
    return this->get(nonConstStr.getTerminatedBuffer(), status);
}

/*
 * freeze().   Close the hash table that maps to the pooled strings.
 *             After freezing, the pool can not be searched or added to,
 *             but all existing references to pooled strings remain valid.
 *
 *             The main purpose is to recover the storage used for the hash.
 */
void ZNStringPool::freeze() {
    uhash_close(fHash);
    fHash = NULL;
}


/**
 * This class stores name data for a meta zone or time zone.
 */
class ZNames : public UMemory {
private:
    friend class TimeZoneNamesImpl;

    static UTimeZoneNameTypeIndex getTZNameTypeIndex(UTimeZoneNameType type) {
        switch(type) {
        case UTZNM_EXEMPLAR_LOCATION: return UTZNM_INDEX_EXEMPLAR_LOCATION;
        case UTZNM_LONG_GENERIC: return UTZNM_INDEX_LONG_GENERIC;
        case UTZNM_LONG_STANDARD: return UTZNM_INDEX_LONG_STANDARD;
        case UTZNM_LONG_DAYLIGHT: return UTZNM_INDEX_LONG_DAYLIGHT;
        case UTZNM_SHORT_GENERIC: return UTZNM_INDEX_SHORT_GENERIC;
        case UTZNM_SHORT_STANDARD: return UTZNM_INDEX_SHORT_STANDARD;
        case UTZNM_SHORT_DAYLIGHT: return UTZNM_INDEX_SHORT_DAYLIGHT;
        default: return UTZNM_INDEX_UNKNOWN;
        }
    }
    static UTimeZoneNameType getTZNameType(UTimeZoneNameTypeIndex index) {
        switch(index) {
        case UTZNM_INDEX_EXEMPLAR_LOCATION: return UTZNM_EXEMPLAR_LOCATION;
        case UTZNM_INDEX_LONG_GENERIC: return UTZNM_LONG_GENERIC;
        case UTZNM_INDEX_LONG_STANDARD: return UTZNM_LONG_STANDARD;
        case UTZNM_INDEX_LONG_DAYLIGHT: return UTZNM_LONG_DAYLIGHT;
        case UTZNM_INDEX_SHORT_GENERIC: return UTZNM_SHORT_GENERIC;
        case UTZNM_INDEX_SHORT_STANDARD: return UTZNM_SHORT_STANDARD;
        case UTZNM_INDEX_SHORT_DAYLIGHT: return UTZNM_SHORT_DAYLIGHT;
        default: return UTZNM_UNKNOWN;
        }
    }

    const UChar* fNames[UTZNM_INDEX_COUNT];
    UBool fDidAddIntoTrie;

    // Whether we own the location string, if computed rather than loaded from a bundle.
    // A meta zone names instance never has an exemplar location string.
    UBool fOwnsLocationName;

    ZNames(const UChar* names[], const UChar* locationName)
            : fDidAddIntoTrie(FALSE) {
        uprv_memcpy(fNames, names, sizeof(fNames));
        if (locationName != NULL) {
            fOwnsLocationName = TRUE;
            fNames[UTZNM_INDEX_EXEMPLAR_LOCATION] = locationName;
        } else {
            fOwnsLocationName = FALSE;
        }
    }

public:
    ~ZNames() {
        if (fOwnsLocationName) {
            const UChar* locationName = fNames[UTZNM_INDEX_EXEMPLAR_LOCATION];
            U_ASSERT(locationName != NULL);
            uprv_free((void*) locationName);
        }
    }

private:
    static void* createMetaZoneAndPutInCache(UHashtable* cache, const UChar* names[],
            const UnicodeString& mzID, UErrorCode& status) {
        if (U_FAILURE(status)) { return NULL; }
        U_ASSERT(names != NULL);

        // Use the persistent ID as the resource key, so we can
        // avoid duplications.
        // TODO: Is there a more efficient way, like intern() in Java?
        void* key = (void*) ZoneMeta::findMetaZoneID(mzID);
        void* value;
        if (uprv_memcmp(names, EMPTY_NAMES, sizeof(EMPTY_NAMES)) == 0) {
            value = (void*) EMPTY;
        } else {
            value = (void*) (new ZNames(names, NULL));
            if (value == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }
        }
        uhash_put(cache, key, value, &status);
        return value;
    }

    static void* createTimeZoneAndPutInCache(UHashtable* cache, const UChar* names[],
            const UnicodeString& tzID, UErrorCode& status) {
        if (U_FAILURE(status)) { return NULL; }
        U_ASSERT(names != NULL);

        // If necessary, compute the location name from the time zone name.
        UChar* locationName = NULL;
        if (names[UTZNM_INDEX_EXEMPLAR_LOCATION] == NULL) {
            UnicodeString locationNameUniStr;
            TimeZoneNamesImpl::getDefaultExemplarLocationName(tzID, locationNameUniStr);

            // Copy the computed location name to the heap
            if (locationNameUniStr.length() > 0) {
                const UChar* buff = locationNameUniStr.getTerminatedBuffer();
                int32_t len = sizeof(UChar) * (locationNameUniStr.length() + 1);
                locationName = (UChar*) uprv_malloc(len);
                if (locationName == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    return NULL;
                }
                uprv_memcpy(locationName, buff, len);
            }
        }

        // Use the persistent ID as the resource key, so we can
        // avoid duplications.
        // TODO: Is there a more efficient way, like intern() in Java?
        void* key = (void*) ZoneMeta::findTimeZoneID(tzID);
        void* value = (void*) (new ZNames(names, locationName));
        if (value == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        uhash_put(cache, key, value, &status);
        return value;
    }

    const UChar* getName(UTimeZoneNameType type) const {
        UTimeZoneNameTypeIndex index = getTZNameTypeIndex(type);
        return index >= 0 ? fNames[index] : NULL;
    }

    void addAsMetaZoneIntoTrie(const UChar* mzID, TextTrieMap& trie, UErrorCode& status) {
        addNamesIntoTrie(mzID, NULL, trie, status);
    }
    void addAsTimeZoneIntoTrie(const UChar* tzID, TextTrieMap& trie, UErrorCode& status) {
        addNamesIntoTrie(NULL, tzID, trie, status);
    }

    void addNamesIntoTrie(const UChar* mzID, const UChar* tzID, TextTrieMap& trie,
            UErrorCode& status) {
        if (U_FAILURE(status)) { return; }
        if (fDidAddIntoTrie) { return; }
        fDidAddIntoTrie = TRUE;

        for (int32_t i = 0; i < UTZNM_INDEX_COUNT; i++) {
            const UChar* name = fNames[i];
            if (name != NULL) {
                ZNameInfo *nameinfo = (ZNameInfo *)uprv_malloc(sizeof(ZNameInfo));
                if (nameinfo == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    return;
                }
                nameinfo->mzID = mzID;
                nameinfo->tzID = tzID;
                nameinfo->type = getTZNameType((UTimeZoneNameTypeIndex)i);
                trie.put(name, nameinfo, status); // trie.put() takes ownership of the key
                if (U_FAILURE(status)) {
                    return;
                }
            }
        }
    }

public:
    struct ZNamesLoader;
};

struct ZNames::ZNamesLoader : public ResourceSink {
    const UChar *names[UTZNM_INDEX_COUNT];

    ZNamesLoader() {
        clear();
    }
    virtual ~ZNamesLoader();

    /** Reset for loading another set of names. */
    void clear() {
        uprv_memcpy(names, EMPTY_NAMES, sizeof(names));
    }

    void loadMetaZone(const UResourceBundle* zoneStrings, const UnicodeString& mzID, UErrorCode& errorCode) {
        if (U_FAILURE(errorCode)) { return; }

        char key[ZID_KEY_MAX + 1];
        mergeTimeZoneKey(mzID, key);

        loadNames(zoneStrings, key, errorCode);
    }

    void loadTimeZone(const UResourceBundle* zoneStrings, const UnicodeString& tzID, UErrorCode& errorCode) {
        // Replace "/" with ":".
        UnicodeString uKey(tzID);
        for (int32_t i = 0; i < uKey.length(); i++) {
            if (uKey.charAt(i) == (UChar)0x2F) {
                uKey.setCharAt(i, (UChar)0x3A);
            }
        }

        char key[ZID_KEY_MAX + 1];
        uKey.extract(0, uKey.length(), key, sizeof(key), US_INV);

        loadNames(zoneStrings, key, errorCode);
    }

    void loadNames(const UResourceBundle* zoneStrings, const char* key, UErrorCode& errorCode) {
        U_ASSERT(zoneStrings != NULL);
        U_ASSERT(key != NULL);
        U_ASSERT(key[0] != '\0');

        UErrorCode localStatus = U_ZERO_ERROR;
        clear();
        ures_getAllItemsWithFallback(zoneStrings, key, *this, localStatus);

        // Ignore errors, but propogate possible warnings.
        if (U_SUCCESS(localStatus)) {
            errorCode = localStatus;
        }
    }

    void setNameIfEmpty(const char* key, const ResourceValue* value, UErrorCode& errorCode) {
        UTimeZoneNameTypeIndex type = nameTypeFromKey(key);
        if (type == UTZNM_INDEX_UNKNOWN) { return; }
        if (names[type] == NULL) {
            int32_t length;
            // 'NO_NAME' indicates internally that this field should remain empty.  It will be
            // replaced by 'NULL' in getNames()
            names[type] = (value == NULL) ? NO_NAME : value->getString(length, errorCode);
        }
    }

    virtual void put(const char* key, ResourceValue& value, UBool /*noFallback*/,
            UErrorCode &errorCode) {
        ResourceTable namesTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int32_t i = 0; namesTable.getKeyAndValue(i, key, value); ++i) {
            if (value.isNoInheritanceMarker()) {
                setNameIfEmpty(key, NULL, errorCode);
            } else {
                setNameIfEmpty(key, &value, errorCode);
            }
        }
    }

    static UTimeZoneNameTypeIndex nameTypeFromKey(const char *key) {
        char c0, c1;
        if ((c0 = key[0]) == 0 || (c1 = key[1]) == 0 || key[2] != 0) {
            return UTZNM_INDEX_UNKNOWN;
        }
        if (c0 == 'l') {
            return c1 == 'g' ? UTZNM_INDEX_LONG_GENERIC :
                    c1 == 's' ? UTZNM_INDEX_LONG_STANDARD :
                        c1 == 'd' ? UTZNM_INDEX_LONG_DAYLIGHT : UTZNM_INDEX_UNKNOWN;
        } else if (c0 == 's') {
            return c1 == 'g' ? UTZNM_INDEX_SHORT_GENERIC :
                    c1 == 's' ? UTZNM_INDEX_SHORT_STANDARD :
                        c1 == 'd' ? UTZNM_INDEX_SHORT_DAYLIGHT : UTZNM_INDEX_UNKNOWN;
        } else if (c0 == 'e' && c1 == 'c') {
            return UTZNM_INDEX_EXEMPLAR_LOCATION;
        }
        return UTZNM_INDEX_UNKNOWN;
    }

    /**
    * Returns an array of names.  It is the caller's responsibility to copy the data into a
    * permanent location, as the returned array is owned by the loader instance and may be
    * cleared or leave scope.
    *
    * This is different than Java, where the array will no longer be modified and null
    * may be returned.
    */
    const UChar** getNames() {
        // Remove 'NO_NAME' references in the array and replace with 'NULL'
        for (int32_t i = 0; i < UTZNM_INDEX_COUNT; ++i) {
            if (names[i] == NO_NAME) {
                names[i] = NULL;
            }
        }
        return names;
    }
};

ZNames::ZNamesLoader::~ZNamesLoader() {}


// ---------------------------------------------------
// The meta zone ID enumeration class
// ---------------------------------------------------
class MetaZoneIDsEnumeration : public StringEnumeration {
public:
    MetaZoneIDsEnumeration();
    MetaZoneIDsEnumeration(const UVector& mzIDs);
    MetaZoneIDsEnumeration(UVector* mzIDs);
    virtual ~MetaZoneIDsEnumeration();
    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
    virtual const UnicodeString* snext(UErrorCode& status);
    virtual void reset(UErrorCode& status);
    virtual int32_t count(UErrorCode& status) const;
private:
    int32_t fLen;
    int32_t fPos;
    const UVector* fMetaZoneIDs;
    UVector *fLocalVector;
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MetaZoneIDsEnumeration)

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration()
: fLen(0), fPos(0), fMetaZoneIDs(NULL), fLocalVector(NULL) {
}

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration(const UVector& mzIDs)
: fPos(0), fMetaZoneIDs(&mzIDs), fLocalVector(NULL) {
    fLen = fMetaZoneIDs->size();
}

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration(UVector *mzIDs)
: fLen(0), fPos(0), fMetaZoneIDs(mzIDs), fLocalVector(mzIDs) {
    if (fMetaZoneIDs) {
        fLen = fMetaZoneIDs->size();
    }
}

const UnicodeString*
MetaZoneIDsEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && fMetaZoneIDs != NULL && fPos < fLen) {
        unistr.setTo((const UChar*)fMetaZoneIDs->elementAt(fPos++), -1);
        return &unistr;
    }
    return NULL;
}

void
MetaZoneIDsEnumeration::reset(UErrorCode& /*status*/) {
    fPos = 0;
}

int32_t
MetaZoneIDsEnumeration::count(UErrorCode& /*status*/) const {
    return fLen;
}

MetaZoneIDsEnumeration::~MetaZoneIDsEnumeration() {
    if (fLocalVector) {
        delete fLocalVector;
    }
}


// ---------------------------------------------------
// ZNameSearchHandler
// ---------------------------------------------------
class ZNameSearchHandler : public TextTrieMapSearchResultHandler {
public:
    ZNameSearchHandler(uint32_t types);
    virtual ~ZNameSearchHandler();

    UBool handleMatch(int32_t matchLength, const CharacterNode *node, UErrorCode &status);
    TimeZoneNames::MatchInfoCollection* getMatches(int32_t& maxMatchLen);

private:
    uint32_t fTypes;
    int32_t fMaxMatchLen;
    TimeZoneNames::MatchInfoCollection* fResults;
};

ZNameSearchHandler::ZNameSearchHandler(uint32_t types)
: fTypes(types), fMaxMatchLen(0), fResults(NULL) {
}

ZNameSearchHandler::~ZNameSearchHandler() {
    if (fResults != NULL) {
        delete fResults;
    }
}

UBool
ZNameSearchHandler::handleMatch(int32_t matchLength, const CharacterNode *node, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (node->hasValues()) {
        int32_t valuesCount = node->countValues();
        for (int32_t i = 0; i < valuesCount; i++) {
            ZNameInfo *nameinfo = (ZNameInfo *)node->getValue(i);
            if (nameinfo == NULL) {
                continue;
            }
            if ((nameinfo->type & fTypes) != 0) {
                // matches a requested type
                if (fResults == NULL) {
                    fResults = new TimeZoneNames::MatchInfoCollection();
                    if (fResults == NULL) {
                        status = U_MEMORY_ALLOCATION_ERROR;
                    }
                }
                if (U_SUCCESS(status)) {
                    U_ASSERT(fResults != NULL);
                    if (nameinfo->tzID) {
                        fResults->addZone(nameinfo->type, matchLength, UnicodeString(nameinfo->tzID, -1), status);
                    } else {
                        U_ASSERT(nameinfo->mzID);
                        fResults->addMetaZone(nameinfo->type, matchLength, UnicodeString(nameinfo->mzID, -1), status);
                    }
                    if (U_SUCCESS(status) && matchLength > fMaxMatchLen) {
                        fMaxMatchLen = matchLength;
                    }
                }
            }
        }
    }
    return TRUE;
}

TimeZoneNames::MatchInfoCollection*
ZNameSearchHandler::getMatches(int32_t& maxMatchLen) {
    // give the ownership to the caller
    TimeZoneNames::MatchInfoCollection* results = fResults;
    maxMatchLen = fMaxMatchLen;

    // reset
    fResults = NULL;
    fMaxMatchLen = 0;
    return results;
}

// ---------------------------------------------------
// TimeZoneNamesImpl
//
// TimeZoneNames implementation class. This is the main
// part of this module.
// ---------------------------------------------------

U_CDECL_BEGIN
/**
 * Deleter for ZNames
 */
static void U_CALLCONV
deleteZNames(void *obj) {
    if (obj != EMPTY) {
        delete (ZNames*) obj;
    }
}

/**
 * Deleter for ZNameInfo
 */
static void U_CALLCONV
deleteZNameInfo(void *obj) {
    uprv_free(obj);
}

U_CDECL_END

TimeZoneNamesImpl::TimeZoneNamesImpl(const Locale& locale, UErrorCode& status)
: fLocale(locale),
  fZoneStrings(NULL),
  fTZNamesMap(NULL),
  fMZNamesMap(NULL),
  fNamesTrieFullyLoaded(FALSE),
  fNamesFullyLoaded(FALSE),
  fNamesTrie(TRUE, deleteZNameInfo) {
    initialize(locale, status);
}

void
TimeZoneNamesImpl::initialize(const Locale& locale, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }

    // Load zoneStrings bundle
    UErrorCode tmpsts = U_ZERO_ERROR;   // OK with fallback warning..
    fZoneStrings = ures_open(U_ICUDATA_ZONE, locale.getName(), &tmpsts);
    fZoneStrings = ures_getByKeyWithFallback(fZoneStrings, gZoneStrings, fZoneStrings, &tmpsts);
    if (U_FAILURE(tmpsts)) {
        status = tmpsts;
        cleanup();
        return;
    }

    // Initialize hashtables holding time zone/meta zone names
    fMZNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    fTZNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    if (U_FAILURE(status)) {
        cleanup();
        return;
    }

    uhash_setValueDeleter(fMZNamesMap, deleteZNames);
    uhash_setValueDeleter(fTZNamesMap, deleteZNames);
    // no key deleters for name maps

    // preload zone strings for the default zone
    TimeZone *tz = TimeZone::createDefault();
    const UChar *tzID = ZoneMeta::getCanonicalCLDRID(*tz);
    if (tzID != NULL) {
        loadStrings(UnicodeString(tzID), status);
    }
    delete tz;

    return;
}

/*
 * This method updates the cache and must be called with a lock,
 * except initializer.
 */
void
TimeZoneNamesImpl::loadStrings(const UnicodeString& tzCanonicalID, UErrorCode& status) {
    loadTimeZoneNames(tzCanonicalID, status);
    LocalPointer<StringEnumeration> mzIDs(getAvailableMetaZoneIDs(tzCanonicalID, status));
    if (U_FAILURE(status)) { return; }
    U_ASSERT(!mzIDs.isNull());

    const UnicodeString *mzID;
    while ((mzID = mzIDs->snext(status)) && U_SUCCESS(status)) {
        loadMetaZoneNames(*mzID, status);
    }
}

TimeZoneNamesImpl::~TimeZoneNamesImpl() {
    cleanup();
}

void
TimeZoneNamesImpl::cleanup() {
    if (fZoneStrings != NULL) {
        ures_close(fZoneStrings);
        fZoneStrings = NULL;
    }
    if (fMZNamesMap != NULL) {
        uhash_close(fMZNamesMap);
        fMZNamesMap = NULL;
    }
    if (fTZNamesMap != NULL) {
        uhash_close(fTZNamesMap);
        fTZNamesMap = NULL;
    }
}

UBool
TimeZoneNamesImpl::operator==(const TimeZoneNames& other) const {
    if (this == &other) {
        return TRUE;
    }
    // No implementation for now
    return FALSE;
}

TimeZoneNames*
TimeZoneNamesImpl::clone() const {
    UErrorCode status = U_ZERO_ERROR;
    return new TimeZoneNamesImpl(fLocale, status);
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(UErrorCode& status) const {
    return TimeZoneNamesImpl::_getAvailableMetaZoneIDs(status);
}

// static implementation of getAvailableMetaZoneIDs(UErrorCode&)
StringEnumeration*
TimeZoneNamesImpl::_getAvailableMetaZoneIDs(UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const UVector* mzIDs = ZoneMeta::getAvailableMetazoneIDs();
    if (mzIDs == NULL) {
        return new MetaZoneIDsEnumeration();
    }
    return new MetaZoneIDsEnumeration(*mzIDs);
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(const UnicodeString& tzID, UErrorCode& status) const {
    return TimeZoneNamesImpl::_getAvailableMetaZoneIDs(tzID, status);
}

// static implementation of getAvailableMetaZoneIDs(const UnicodeString&, UErrorCode&)
StringEnumeration*
TimeZoneNamesImpl::_getAvailableMetaZoneIDs(const UnicodeString& tzID, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const UVector* mappings = ZoneMeta::getMetazoneMappings(tzID);
    if (mappings == NULL) {
        return new MetaZoneIDsEnumeration();
    }

    MetaZoneIDsEnumeration *senum = NULL;
    UVector* mzIDs = new UVector(NULL, uhash_compareUChars, status);
    if (mzIDs == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_SUCCESS(status)) {
        U_ASSERT(mzIDs != NULL);
        for (int32_t i = 0; U_SUCCESS(status) && i < mappings->size(); i++) {

            OlsonToMetaMappingEntry *map = (OlsonToMetaMappingEntry *)mappings->elementAt(i);
            const UChar *mzID = map->mzid;
            if (!mzIDs->contains((void *)mzID)) {
                mzIDs->addElement((void *)mzID, status);
            }
        }
        if (U_SUCCESS(status)) {
            senum = new MetaZoneIDsEnumeration(mzIDs);
        } else {
            delete mzIDs;
        }
    }
    return senum;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    return TimeZoneNamesImpl::_getMetaZoneID(tzID, date, mzID);
}

// static implementation of getMetaZoneID
UnicodeString&
TimeZoneNamesImpl::_getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) {
    ZoneMeta::getMetazoneID(tzID, date, mzID);
    return mzID;
}

UnicodeString&
TimeZoneNamesImpl::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    return TimeZoneNamesImpl::_getReferenceZoneID(mzID, region, tzID);
}

// static implementaion of getReferenceZoneID
UnicodeString&
TimeZoneNamesImpl::_getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) {
    ZoneMeta::getZoneIdByMetazone(mzID, UnicodeString(region, -1, US_INV), tzID);
    return tzID;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneDisplayName(const UnicodeString& mzID,
                                          UTimeZoneNameType type,
                                          UnicodeString& name) const {
    name.setToBogus();  // cleanup result.
    if (mzID.isEmpty()) {
        return name;
    }

    ZNames *znames = NULL;
    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl *>(this);

    {
        Mutex lock(&gDataMutex);
        UErrorCode status = U_ZERO_ERROR;
        znames = nonConstThis->loadMetaZoneNames(mzID, status);
        if (U_FAILURE(status)) { return name; }
    }

    if (znames != NULL) {
        const UChar* s = znames->getName(type);
        if (s != NULL) {
            name.setTo(TRUE, s, -1);
        }
    }
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const {
    name.setToBogus();  // cleanup result.
    if (tzID.isEmpty()) {
        return name;
    }

    ZNames *tznames = NULL;
    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl *>(this);

    {
        Mutex lock(&gDataMutex);
        UErrorCode status = U_ZERO_ERROR;
        tznames = nonConstThis->loadTimeZoneNames(tzID, status);
        if (U_FAILURE(status)) { return name; }
    }

    if (tznames != NULL) {
        const UChar *s = tznames->getName(type);
        if (s != NULL) {
            name.setTo(TRUE, s, -1);
        }
    }
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    name.setToBogus();  // cleanup result.
    const UChar* locName = NULL;
    ZNames *tznames = NULL;
    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl *>(this);

    {
        Mutex lock(&gDataMutex);
        UErrorCode status = U_ZERO_ERROR;
        tznames = nonConstThis->loadTimeZoneNames(tzID, status);
        if (U_FAILURE(status)) { return name; }
    }

    if (tznames != NULL) {
        locName = tznames->getName(UTZNM_EXEMPLAR_LOCATION);
    }
    if (locName != NULL) {
        name.setTo(TRUE, locName, -1);
    }

    return name;
}


// Merge the MZ_PREFIX and mzId
static void mergeTimeZoneKey(const UnicodeString& mzID, char* result) {
    if (mzID.isEmpty()) {
        result[0] = '\0';
        return;
    }

    char mzIdChar[ZID_KEY_MAX + 1];
    int32_t keyLen;
    int32_t prefixLen = uprv_strlen(gMZPrefix);
    keyLen = mzID.extract(0, mzID.length(), mzIdChar, ZID_KEY_MAX + 1, US_INV);
    uprv_memcpy((void *)result, (void *)gMZPrefix, prefixLen);
    uprv_memcpy((void *)(result + prefixLen), (void *)mzIdChar, keyLen);
    result[keyLen + prefixLen] = '\0';
}

/*
 * This method updates the cache and must be called with a lock
 */
ZNames*
TimeZoneNamesImpl::loadMetaZoneNames(const UnicodeString& mzID, UErrorCode& status) {
    if (U_FAILURE(status)) { return NULL; }
    U_ASSERT(mzID.length() <= ZID_KEY_MAX - MZ_PREFIX_LEN);

    UChar mzIDKey[ZID_KEY_MAX + 1];
    mzID.extract(mzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(U_SUCCESS(status));   // already checked length above
    mzIDKey[mzID.length()] = 0;

    void* mznames = uhash_get(fMZNamesMap, mzIDKey);
    if (mznames == NULL) {
        ZNames::ZNamesLoader loader;
        loader.loadMetaZone(fZoneStrings, mzID, status);
        mznames = ZNames::createMetaZoneAndPutInCache(fMZNamesMap, loader.getNames(), mzID, status);
        if (U_FAILURE(status)) { return NULL; }
    }

    if (mznames != EMPTY) {
        return (ZNames*)mznames;
    } else {
        return NULL;
    }
}

/*
 * This method updates the cache and must be called with a lock
 */
ZNames*
TimeZoneNamesImpl::loadTimeZoneNames(const UnicodeString& tzID, UErrorCode& status) {
    if (U_FAILURE(status)) { return NULL; }
    U_ASSERT(tzID.length() <= ZID_KEY_MAX);

    UChar tzIDKey[ZID_KEY_MAX + 1];
    int32_t tzIDKeyLen = tzID.extract(tzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(U_SUCCESS(status));   // already checked length above
    tzIDKey[tzIDKeyLen] = 0;

    void *tznames = uhash_get(fTZNamesMap, tzIDKey);
    if (tznames == NULL) {
        ZNames::ZNamesLoader loader;
        loader.loadTimeZone(fZoneStrings, tzID, status);
        tznames = ZNames::createTimeZoneAndPutInCache(fTZNamesMap, loader.getNames(), tzID, status);
        if (U_FAILURE(status)) { return NULL; }
    }

    // tznames is never EMPTY
    return (ZNames*)tznames;
}

TimeZoneNames::MatchInfoCollection*
TimeZoneNamesImpl::find(const UnicodeString& text, int32_t start, uint32_t types, UErrorCode& status) const {
    ZNameSearchHandler handler(types);
    TimeZoneNames::MatchInfoCollection* matches;
    TimeZoneNamesImpl* nonConstThis = const_cast<TimeZoneNamesImpl*>(this);

    // Synchronize so that data is not loaded multiple times.
    // TODO: Consider more fine-grained synchronization.
    {
        Mutex lock(&gDataMutex);

        // First try of lookup.
        matches = doFind(handler, text, start, status);
        if (U_FAILURE(status)) { return NULL; }
        if (matches != NULL) {
            return matches;
        }

        // All names are not yet loaded into the trie.
        // We may have loaded names for formatting several time zones,
        // and might be parsing one of those.
        // Populate the parsing trie from all of the already-loaded names.
        nonConstThis->addAllNamesIntoTrie(status);

        // Second try of lookup.
        matches = doFind(handler, text, start, status);
        if (U_FAILURE(status)) { return NULL; }
        if (matches != NULL) {
            return matches;
        }

        // There are still some names we haven't loaded into the trie yet.
        // Load everything now.
        nonConstThis->internalLoadAllDisplayNames(status);
        nonConstThis->addAllNamesIntoTrie(status);
        nonConstThis->fNamesTrieFullyLoaded = TRUE;
        if (U_FAILURE(status)) { return NULL; }

        // Third try: we must return this one.
        return doFind(handler, text, start, status);
    }
}

TimeZoneNames::MatchInfoCollection*
TimeZoneNamesImpl::doFind(ZNameSearchHandler& handler,
        const UnicodeString& text, int32_t start, UErrorCode& status) const {

    fNamesTrie.search(text, start, (TextTrieMapSearchResultHandler *)&handler, status);
    if (U_FAILURE(status)) { return NULL; }

    int32_t maxLen = 0;
    TimeZoneNames::MatchInfoCollection* matches = handler.getMatches(maxLen);
    if (matches != NULL && ((maxLen == (text.length() - start)) || fNamesTrieFullyLoaded)) {
        // perfect match, or no more names available
        return matches;
    }
    delete matches;
    return NULL;
}

// Caller must synchronize.
void TimeZoneNamesImpl::addAllNamesIntoTrie(UErrorCode& status) {
    if (U_FAILURE(status)) return;
    int32_t pos;
    const UHashElement* element;

    pos = UHASH_FIRST;
    while ((element = uhash_nextElement(fMZNamesMap, &pos)) != NULL) {
        if (element->value.pointer == EMPTY) { continue; }
        UChar* mzID = (UChar*) element->key.pointer;
        ZNames* znames = (ZNames*) element->value.pointer;
        znames->addAsMetaZoneIntoTrie(mzID, fNamesTrie, status);
        if (U_FAILURE(status)) { return; }
    }

    pos = UHASH_FIRST;
    while ((element = uhash_nextElement(fTZNamesMap, &pos)) != NULL) {
        if (element->value.pointer == EMPTY) { continue; }
        UChar* tzID = (UChar*) element->key.pointer;
        ZNames* znames = (ZNames*) element->value.pointer;
        znames->addAsTimeZoneIntoTrie(tzID, fNamesTrie, status);
        if (U_FAILURE(status)) { return; }
    }
}

U_CDECL_BEGIN
static void U_CALLCONV
deleteZNamesLoader(void* obj) {
    if (obj == DUMMY_LOADER) { return; }
    const ZNames::ZNamesLoader* loader = (const ZNames::ZNamesLoader*) obj;
    delete loader;
}
U_CDECL_END

struct TimeZoneNamesImpl::ZoneStringsLoader : public ResourceSink {
    TimeZoneNamesImpl& tzn;
    UHashtable* keyToLoader;

    ZoneStringsLoader(TimeZoneNamesImpl& _tzn, UErrorCode& status)
            : tzn(_tzn) {
        keyToLoader = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
        if (U_FAILURE(status)) { return; }
        uhash_setKeyDeleter(keyToLoader, uprv_free);
        uhash_setValueDeleter(keyToLoader, deleteZNamesLoader);
    }
    virtual ~ZoneStringsLoader();

    void* createKey(const char* key, UErrorCode& status) {
        int32_t len = sizeof(char) * (uprv_strlen(key) + 1);
        char* newKey = (char*) uprv_malloc(len);
        if (newKey == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        uprv_memcpy(newKey, key, len);
        newKey[len-1] = '\0';
        return (void*) newKey;
    }

    UBool isMetaZone(const char* key) {
        return (uprv_strlen(key) >= MZ_PREFIX_LEN && uprv_memcmp(key, gMZPrefix, MZ_PREFIX_LEN) == 0);
    }

    UnicodeString mzIDFromKey(const char* key) {
        return UnicodeString(key + MZ_PREFIX_LEN, uprv_strlen(key) - MZ_PREFIX_LEN, US_INV);
    }

    UnicodeString tzIDFromKey(const char* key) {
        UnicodeString tzID(key, -1, US_INV);
        // Replace all colons ':' with slashes '/'
        for (int i=0; i<tzID.length(); i++) {
            if (tzID.charAt(i) == 0x003A) {
                tzID.setCharAt(i, 0x002F);
            }
        }
        return tzID;
    }

    void load(UErrorCode& status) {
        ures_getAllItemsWithFallback(tzn.fZoneStrings, "", *this, status);
        if (U_FAILURE(status)) { return; }

        int32_t pos = UHASH_FIRST;
        const UHashElement* element;
        while ((element = uhash_nextElement(keyToLoader, &pos)) != NULL) {
            if (element->value.pointer == DUMMY_LOADER) { continue; }
            ZNames::ZNamesLoader* loader = (ZNames::ZNamesLoader*) element->value.pointer;
            char* key = (char*) element->key.pointer;

            if (isMetaZone(key)) {
                UnicodeString mzID = mzIDFromKey(key);
                ZNames::createMetaZoneAndPutInCache(tzn.fMZNamesMap, loader->getNames(), mzID, status);
            } else {
                UnicodeString tzID = tzIDFromKey(key);
                ZNames::createTimeZoneAndPutInCache(tzn.fTZNamesMap, loader->getNames(), tzID, status);
            }
            if (U_FAILURE(status)) { return; }
        }
    }

    void consumeNamesTable(const char *key, ResourceValue &value, UBool noFallback,
            UErrorCode &status) {
        if (U_FAILURE(status)) { return; }

        void* loader = uhash_get(keyToLoader, key);
        if (loader == NULL) {
            if (isMetaZone(key)) {
                UnicodeString mzID = mzIDFromKey(key);
                void* cacheVal = uhash_get(tzn.fMZNamesMap, mzID.getTerminatedBuffer());
                if (cacheVal != NULL) {
                    // We have already loaded the names for this meta zone.
                    loader = (void*) DUMMY_LOADER;
                } else {
                    loader = (void*) new ZNames::ZNamesLoader();
                    if (loader == NULL) {
                        status = U_MEMORY_ALLOCATION_ERROR;
                        return;
                    }
                }
            } else {
                UnicodeString tzID = tzIDFromKey(key);
                void* cacheVal = uhash_get(tzn.fTZNamesMap, tzID.getTerminatedBuffer());
                if (cacheVal != NULL) {
                    // We have already loaded the names for this time zone.
                    loader = (void*) DUMMY_LOADER;
                } else {
                    loader = (void*) new ZNames::ZNamesLoader();
                    if (loader == NULL) {
                        status = U_MEMORY_ALLOCATION_ERROR;
                        return;
                    }
                }
            }

            void* newKey = createKey(key, status);
            if (U_FAILURE(status)) {
                deleteZNamesLoader(loader);
                return;
            }

            uhash_put(keyToLoader, newKey, loader, &status);
            if (U_FAILURE(status)) { return; }
        }

        if (loader != DUMMY_LOADER) {
            // Let the ZNamesLoader consume the names table.
            ((ZNames::ZNamesLoader*)loader)->put(key, value, noFallback, status);
        }
    }

    virtual void put(const char *key, ResourceValue &value, UBool noFallback,
            UErrorCode &status) {
        ResourceTable timeZonesTable = value.getTable(status);
        if (U_FAILURE(status)) { return; }
        for (int32_t i = 0; timeZonesTable.getKeyAndValue(i, key, value); ++i) {
            U_ASSERT(!value.isNoInheritanceMarker());
            if (value.getType() == URES_TABLE) {
                consumeNamesTable(key, value, noFallback, status);
            } else {
                // Ignore fields that aren't tables (e.g., fallbackFormat and regionFormatStandard).
                // All time zone fields are tables.
            }
            if (U_FAILURE(status)) { return; }
        }
    }
};

// Virtual destructors must be defined out of line.
TimeZoneNamesImpl::ZoneStringsLoader::~ZoneStringsLoader() {
    uhash_close(keyToLoader);
}

void TimeZoneNamesImpl::loadAllDisplayNames(UErrorCode& status) {
    if (U_FAILURE(status)) return;

    {
        Mutex lock(&gDataMutex);
        internalLoadAllDisplayNames(status);
    }
}

void TimeZoneNamesImpl::getDisplayNames(const UnicodeString& tzID,
        const UTimeZoneNameType types[], int32_t numTypes,
        UDate date, UnicodeString dest[], UErrorCode& status) const {
    if (U_FAILURE(status)) return;

    if (tzID.isEmpty()) { return; }
    void* tznames = NULL;
    void* mznames = NULL;
    TimeZoneNamesImpl *nonConstThis = const_cast<TimeZoneNamesImpl*>(this);

    // Load the time zone strings
    {
        Mutex lock(&gDataMutex);
        tznames = (void*) nonConstThis->loadTimeZoneNames(tzID, status);
        if (U_FAILURE(status)) { return; }
    }
    U_ASSERT(tznames != NULL);

    // Load the values into the dest array
    for (int i = 0; i < numTypes; i++) {
        UTimeZoneNameType type = types[i];
        const UChar* name = ((ZNames*)tznames)->getName(type);
        if (name == NULL) {
            if (mznames == NULL) {
                // Load the meta zone name
                UnicodeString mzID;
                getMetaZoneID(tzID, date, mzID);
                if (mzID.isEmpty()) {
                    mznames = (void*) EMPTY;
                } else {
                    // Load the meta zone strings
                    // Mutex is scoped to the "else" statement
                    Mutex lock(&gDataMutex);
                    mznames = (void*) nonConstThis->loadMetaZoneNames(mzID, status);
                    if (U_FAILURE(status)) { return; }
                    // Note: when the metazone doesn't exist, in Java, loadMetaZoneNames returns
                    // a dummy object instead of NULL.
                    if (mznames == NULL) {
                        mznames = (void*) EMPTY;
                    }
                }
            }
            U_ASSERT(mznames != NULL);
            if (mznames != EMPTY) {
                name = ((ZNames*)mznames)->getName(type);
            }
        }
        if (name != NULL) {
            dest[i].setTo(TRUE, name, -1);
        } else {
            dest[i].setToBogus();
        }
    }
}

// Caller must synchronize.
void TimeZoneNamesImpl::internalLoadAllDisplayNames(UErrorCode& status) {
    if (!fNamesFullyLoaded) {
        fNamesFullyLoaded = TRUE;

        ZoneStringsLoader loader(*this, status);
        loader.load(status);
        if (U_FAILURE(status)) { return; }

        const UnicodeString *id;

        // load strings for all zones
        StringEnumeration *tzIDs = TimeZone::createTimeZoneIDEnumeration(
            UCAL_ZONE_TYPE_CANONICAL, NULL, NULL, status);
        if (U_SUCCESS(status)) {
            while ((id = tzIDs->snext(status))) {
                if (U_FAILURE(status)) {
                    break;
                }
                UnicodeString copy(*id);
                void* value = uhash_get(fTZNamesMap, copy.getTerminatedBuffer());
                if (value == NULL) {
                    // loadStrings also loads related metazone strings
                    loadStrings(*id, status);
                }
            }
        }
        if (tzIDs != NULL) {
            delete tzIDs;
        }
    }
}



static const UChar gEtcPrefix[]         = { 0x45, 0x74, 0x63, 0x2F }; // "Etc/"
static const int32_t gEtcPrefixLen      = 4;
static const UChar gSystemVPrefix[]     = { 0x53, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x56, 0x2F }; // "SystemV/
static const int32_t gSystemVPrefixLen  = 8;
static const UChar gRiyadh8[]           = { 0x52, 0x69, 0x79, 0x61, 0x64, 0x68, 0x38 }; // "Riyadh8"
static const int32_t gRiyadh8Len       = 7;

UnicodeString& U_EXPORT2
TimeZoneNamesImpl::getDefaultExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) {
    if (tzID.isEmpty() || tzID.startsWith(gEtcPrefix, gEtcPrefixLen)
        || tzID.startsWith(gSystemVPrefix, gSystemVPrefixLen) || tzID.indexOf(gRiyadh8, gRiyadh8Len, 0) > 0) {
        name.setToBogus();
        return name;
    }

    int32_t sep = tzID.lastIndexOf((UChar)0x2F /* '/' */);
    if (sep > 0 && sep + 1 < tzID.length()) {
        name.setTo(tzID, sep + 1);
        name.findAndReplace(UnicodeString((UChar)0x5f /* _ */),
                            UnicodeString((UChar)0x20 /* space */));
    } else {
        name.setToBogus();
    }
    return name;
}

// ---------------------------------------------------
// TZDBTimeZoneNames and its supporting classes
//
// TZDBTimeZoneNames is an implementation class of
// TimeZoneNames holding the IANA tz database abbreviations.
// ---------------------------------------------------

class TZDBNames : public UMemory {
public:
    virtual ~TZDBNames();

    static TZDBNames* createInstance(UResourceBundle* rb, const char* key);
    const UChar* getName(UTimeZoneNameType type) const;
    const char** getParseRegions(int32_t& numRegions) const;

protected:
    TZDBNames(const UChar** names, char** regions, int32_t numRegions);

private:
    const UChar** fNames;
    char** fRegions;
    int32_t fNumRegions;
};

TZDBNames::TZDBNames(const UChar** names, char** regions, int32_t numRegions)
    :   fNames(names),
        fRegions(regions),
        fNumRegions(numRegions) {
}

TZDBNames::~TZDBNames() {
    if (fNames != NULL) {
        uprv_free(fNames);
    }
    if (fRegions != NULL) {
        char **p = fRegions;
        for (int32_t i = 0; i < fNumRegions; p++, i++) {
            uprv_free(*p);
        }
        uprv_free(fRegions);
    }
}

TZDBNames*
TZDBNames::createInstance(UResourceBundle* rb, const char* key) {
    if (rb == NULL || key == NULL || *key == 0) {
        return NULL;
    }

    UErrorCode status = U_ZERO_ERROR;

    const UChar **names = NULL;
    char** regions = NULL;
    int32_t numRegions = 0;

    int32_t len = 0;

    UResourceBundle* rbTable = NULL;
    rbTable = ures_getByKey(rb, key, rbTable, &status);
    if (U_FAILURE(status)) {
        return NULL;
    }

    names = (const UChar **)uprv_malloc(sizeof(const UChar*) * TZDBNAMES_KEYS_SIZE);
    UBool isEmpty = TRUE;
    if (names != NULL) {
        for (int32_t i = 0; i < TZDBNAMES_KEYS_SIZE; i++) {
            status = U_ZERO_ERROR;
            const UChar *value = ures_getStringByKey(rbTable, TZDBNAMES_KEYS[i], &len, &status);
            if (U_FAILURE(status) || len == 0) {
                names[i] = NULL;
            } else {
                names[i] = value;
                isEmpty = FALSE;
            }
        }
    }

    if (isEmpty) {
        if (names != NULL) {
            uprv_free(names);
        }
        return NULL;
    }

    UResourceBundle *regionsRes = ures_getByKey(rbTable, "parseRegions", NULL, &status);
    UBool regionError = FALSE;
    if (U_SUCCESS(status)) {
        numRegions = ures_getSize(regionsRes);
        if (numRegions > 0) {
            regions = (char**)uprv_malloc(sizeof(char*) * numRegions);
            if (regions != NULL) {
                char **pRegion = regions;
                for (int32_t i = 0; i < numRegions; i++, pRegion++) {
                    *pRegion = NULL;
                }
                // filling regions
                pRegion = regions;
                for (int32_t i = 0; i < numRegions; i++, pRegion++) {
                    status = U_ZERO_ERROR;
                    const UChar *uregion = ures_getStringByIndex(regionsRes, i, &len, &status);
                    if (U_FAILURE(status)) {
                        regionError = TRUE;
                        break;
                    }
                    *pRegion = (char*)uprv_malloc(sizeof(char) * (len + 1));
                    if (*pRegion == NULL) {
                        regionError = TRUE;
                        break;
                    }
                    u_UCharsToChars(uregion, *pRegion, len);
                    (*pRegion)[len] = 0;
                }
            }
        }
    }
    ures_close(regionsRes);
    ures_close(rbTable);

    if (regionError) {
        if (names != NULL) {
            uprv_free(names);
        }
        if (regions != NULL) {
            char **p = regions;
            for (int32_t i = 0; i < numRegions; p++, i++) {
                uprv_free(*p);
            }
            uprv_free(regions);
        }
        return NULL;
    }

    return new TZDBNames(names, regions, numRegions);
}

const UChar*
TZDBNames::getName(UTimeZoneNameType type) const {
    if (fNames == NULL) {
        return NULL;
    }
    const UChar *name = NULL;
    switch(type) {
    case UTZNM_SHORT_STANDARD:
        name = fNames[0];
        break;
    case UTZNM_SHORT_DAYLIGHT:
        name = fNames[1];
        break;
    default:
        name = NULL;
    }
    return name;
}

const char**
TZDBNames::getParseRegions(int32_t& numRegions) const {
    if (fRegions == NULL) {
        numRegions = 0;
    } else {
        numRegions = fNumRegions;
    }
    return (const char**)fRegions;
}

U_CDECL_BEGIN
/**
 * TZDBNameInfo stores metazone name information for the IANA abbreviations
 * in the trie
 */
typedef struct TZDBNameInfo {
    const UChar*        mzID;
    UTimeZoneNameType   type;
    UBool               ambiguousType;
    const char**        parseRegions;
    int32_t             nRegions;
} TZDBNameInfo;
U_CDECL_END


class TZDBNameSearchHandler : public TextTrieMapSearchResultHandler {
public:
    TZDBNameSearchHandler(uint32_t types, const char* region);
    virtual ~TZDBNameSearchHandler();

    UBool handleMatch(int32_t matchLength, const CharacterNode *node, UErrorCode &status);
    TimeZoneNames::MatchInfoCollection* getMatches(int32_t& maxMatchLen);

private:
    uint32_t fTypes;
    int32_t fMaxMatchLen;
    TimeZoneNames::MatchInfoCollection* fResults;
    const char* fRegion;
};

TZDBNameSearchHandler::TZDBNameSearchHandler(uint32_t types, const char* region)
: fTypes(types), fMaxMatchLen(0), fResults(NULL), fRegion(region) {
}

TZDBNameSearchHandler::~TZDBNameSearchHandler() {
    if (fResults != NULL) {
        delete fResults;
    }
}

UBool
TZDBNameSearchHandler::handleMatch(int32_t matchLength, const CharacterNode *node, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }

    TZDBNameInfo *match = NULL;
    TZDBNameInfo *defaultRegionMatch = NULL;

    if (node->hasValues()) {
        int32_t valuesCount = node->countValues();
        for (int32_t i = 0; i < valuesCount; i++) {
            TZDBNameInfo *ninfo = (TZDBNameInfo *)node->getValue(i);
            if (ninfo == NULL) {
                continue;
            }
            if ((ninfo->type & fTypes) != 0) {
                // Some tz database abbreviations are ambiguous. For example,
                // CST means either Central Standard Time or China Standard Time.
                // Unlike CLDR time zone display names, this implementation
                // does not use unique names. And TimeZoneFormat does not expect
                // multiple results returned for the same time zone type.
                // For this reason, this implementation resolve one among same
                // zone type with a same name at this level.
                if (ninfo->parseRegions == NULL) {
                    // parseRegions == null means this is the default metazone
                    // mapping for the abbreviation.
                    if (defaultRegionMatch == NULL) {
                        match = defaultRegionMatch = ninfo;
                    }
                } else {
                    UBool matchRegion = FALSE;
                    // non-default metazone mapping for an abbreviation
                    // comes with applicable regions. For example, the default
                    // metazone mapping for "CST" is America_Central,
                    // but if region is one of CN/MO/TW, "CST" is parsed
                    // as metazone China (China Standard Time).
                    for (int32_t i = 0; i < ninfo->nRegions; i++) {
                        const char *region = ninfo->parseRegions[i];
                        if (uprv_strcmp(fRegion, region) == 0) {
                            match = ninfo;
                            matchRegion = TRUE;
                            break;
                        }
                    }
                    if (matchRegion) {
                        break;
                    }
                    if (match == NULL) {
                        match = ninfo;
                    }
                }
            }
        }

        if (match != NULL) {
            UTimeZoneNameType ntype = match->type;
            // Note: Workaround for duplicated standard/daylight names
            // The tz database contains a few zones sharing a
            // same name for both standard time and daylight saving
            // time. For example, Australia/Sydney observes DST,
            // but "EST" is used for both standard and daylight.
            // When both SHORT_STANDARD and SHORT_DAYLIGHT are included
            // in the find operation, we cannot tell which one was
            // actually matched.
            // TimeZoneFormat#parse returns a matched name type (standard
            // or daylight) and DateFormat implementation uses the info to
            // to adjust actual time. To avoid false type information,
            // this implementation replaces the name type with SHORT_GENERIC.
            if (match->ambiguousType
                    && (ntype == UTZNM_SHORT_STANDARD || ntype == UTZNM_SHORT_DAYLIGHT)
                    && (fTypes & UTZNM_SHORT_STANDARD) != 0
                    && (fTypes & UTZNM_SHORT_DAYLIGHT) != 0) {
                ntype = UTZNM_SHORT_GENERIC;
            }

            if (fResults == NULL) {
                fResults = new TimeZoneNames::MatchInfoCollection();
                if (fResults == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                }
            }
            if (U_SUCCESS(status)) {
                U_ASSERT(fResults != NULL);
                U_ASSERT(match->mzID != NULL);
                fResults->addMetaZone(ntype, matchLength, UnicodeString(match->mzID, -1), status);
                if (U_SUCCESS(status) && matchLength > fMaxMatchLen) {
                    fMaxMatchLen = matchLength;
                }
            }
        }
    }
    return TRUE;
}

TimeZoneNames::MatchInfoCollection*
TZDBNameSearchHandler::getMatches(int32_t& maxMatchLen) {
    // give the ownership to the caller
    TimeZoneNames::MatchInfoCollection* results = fResults;
    maxMatchLen = fMaxMatchLen;

    // reset
    fResults = NULL;
    fMaxMatchLen = 0;
    return results;
}

U_CDECL_BEGIN
/**
 * Deleter for TZDBNames
 */
static void U_CALLCONV
deleteTZDBNames(void *obj) {
    if (obj != EMPTY) {
        delete (TZDBNames *)obj;
    }
}

static void U_CALLCONV initTZDBNamesMap(UErrorCode &status) {
    gTZDBNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    if (U_FAILURE(status)) {
        gTZDBNamesMap = NULL;
        return;
    }
    // no key deleters for tzdb name maps
    uhash_setValueDeleter(gTZDBNamesMap, deleteTZDBNames);
    ucln_i18n_registerCleanup(UCLN_I18N_TZDBTIMEZONENAMES, tzdbTimeZoneNames_cleanup);
}

/**
 * Deleter for TZDBNameInfo
 */
static void U_CALLCONV
deleteTZDBNameInfo(void *obj) {
    if (obj != NULL) {
        uprv_free(obj);
    }
}

static void U_CALLCONV prepareFind(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    gTZDBNamesTrie = new TextTrieMap(TRUE, deleteTZDBNameInfo);
    if (gTZDBNamesTrie == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    const UnicodeString *mzID;
    StringEnumeration *mzIDs = TimeZoneNamesImpl::_getAvailableMetaZoneIDs(status);
    if (U_SUCCESS(status)) {
        while ((mzID = mzIDs->snext(status)) && U_SUCCESS(status)) {
            const TZDBNames *names = TZDBTimeZoneNames::getMetaZoneNames(*mzID, status);
            if (names == NULL) {
                continue;
            }
            const UChar *std = names->getName(UTZNM_SHORT_STANDARD);
            const UChar *dst = names->getName(UTZNM_SHORT_DAYLIGHT);
            if (std == NULL && dst == NULL) {
                continue;
            }
            int32_t numRegions = 0;
            const char **parseRegions = names->getParseRegions(numRegions);

            // The tz database contains a few zones sharing a
            // same name for both standard time and daylight saving
            // time. For example, Australia/Sydney observes DST,
            // but "EST" is used for both standard and daylight.
            // we need to store the information for later processing.
            UBool ambiguousType = (std != NULL && dst != NULL && u_strcmp(std, dst) == 0);

            const UChar *uMzID = ZoneMeta::findMetaZoneID(*mzID);
            if (std != NULL) {
                TZDBNameInfo *stdInf = (TZDBNameInfo *)uprv_malloc(sizeof(TZDBNameInfo));
                if (stdInf == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }
                stdInf->mzID = uMzID;
                stdInf->type = UTZNM_SHORT_STANDARD;
                stdInf->ambiguousType = ambiguousType;
                stdInf->parseRegions = parseRegions;
                stdInf->nRegions = numRegions;
                gTZDBNamesTrie->put(std, stdInf, status);
            }
            if (U_SUCCESS(status) && dst != NULL) {
                TZDBNameInfo *dstInf = (TZDBNameInfo *)uprv_malloc(sizeof(TZDBNameInfo));
                if (dstInf == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }
                dstInf->mzID = uMzID;
                dstInf->type = UTZNM_SHORT_DAYLIGHT;
                dstInf->ambiguousType = ambiguousType;
                dstInf->parseRegions = parseRegions;
                dstInf->nRegions = numRegions;
                gTZDBNamesTrie->put(dst, dstInf, status);
            }
        }
    }
    delete mzIDs;

    if (U_FAILURE(status)) {
        delete gTZDBNamesTrie;
        gTZDBNamesTrie = NULL;
        return;
    }

    ucln_i18n_registerCleanup(UCLN_I18N_TZDBTIMEZONENAMES, tzdbTimeZoneNames_cleanup);
}

U_CDECL_END

TZDBTimeZoneNames::TZDBTimeZoneNames(const Locale& locale)
: fLocale(locale) {
    UBool useWorld = TRUE;
    const char* region = fLocale.getCountry();
    int32_t regionLen = uprv_strlen(region);
    if (regionLen == 0) {
        UErrorCode status = U_ZERO_ERROR;
        char loc[ULOC_FULLNAME_CAPACITY];
        uloc_addLikelySubtags(fLocale.getName(), loc, sizeof(loc), &status);
        regionLen = uloc_getCountry(loc, fRegion, sizeof(fRegion), &status);
        if (U_SUCCESS(status) && regionLen < (int32_t)sizeof(fRegion)) {
            useWorld = FALSE;
        }
    } else if (regionLen < (int32_t)sizeof(fRegion)) {
        uprv_strcpy(fRegion, region);
        useWorld = FALSE;
    }
    if (useWorld) {
        uprv_strcpy(fRegion, "001");
    }
}

TZDBTimeZoneNames::~TZDBTimeZoneNames() {
}

UBool
TZDBTimeZoneNames::operator==(const TimeZoneNames& other) const {
    if (this == &other) {
        return TRUE;
    }
    // No implementation for now
    return FALSE;
}

TimeZoneNames*
TZDBTimeZoneNames::clone() const {
    return new TZDBTimeZoneNames(fLocale);
}

StringEnumeration*
TZDBTimeZoneNames::getAvailableMetaZoneIDs(UErrorCode& status) const {
    return TimeZoneNamesImpl::_getAvailableMetaZoneIDs(status);
}

StringEnumeration*
TZDBTimeZoneNames::getAvailableMetaZoneIDs(const UnicodeString& tzID, UErrorCode& status) const {
    return TimeZoneNamesImpl::_getAvailableMetaZoneIDs(tzID, status);
}

UnicodeString&
TZDBTimeZoneNames::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    return TimeZoneNamesImpl::_getMetaZoneID(tzID, date, mzID);
}

UnicodeString&
TZDBTimeZoneNames::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    return TimeZoneNamesImpl::_getReferenceZoneID(mzID, region, tzID);
}

UnicodeString&
TZDBTimeZoneNames::getMetaZoneDisplayName(const UnicodeString& mzID,
                                          UTimeZoneNameType type,
                                          UnicodeString& name) const {
    name.setToBogus();
    if (mzID.isEmpty()) {
        return name;
    }

    UErrorCode status = U_ZERO_ERROR;
    const TZDBNames *tzdbNames = TZDBTimeZoneNames::getMetaZoneNames(mzID, status);
    if (U_SUCCESS(status)) {
        const UChar *s = tzdbNames->getName(type);
        if (s != NULL) {
            name.setTo(TRUE, s, -1);
        }
    }

    return name;
}

UnicodeString&
TZDBTimeZoneNames::getTimeZoneDisplayName(const UnicodeString& /* tzID */, UTimeZoneNameType /* type */, UnicodeString& name) const {
    // No abbreviations associated a zone directly for now.
    name.setToBogus();
    return name;
}

TZDBTimeZoneNames::MatchInfoCollection*
TZDBTimeZoneNames::find(const UnicodeString& text, int32_t start, uint32_t types, UErrorCode& status) const {
    umtx_initOnce(gTZDBNamesTrieInitOnce, &prepareFind, status);
    if (U_FAILURE(status)) {
        return NULL;
    }

    TZDBNameSearchHandler handler(types, fRegion);
    gTZDBNamesTrie->search(text, start, (TextTrieMapSearchResultHandler *)&handler, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    int32_t maxLen = 0;
    return handler.getMatches(maxLen);
}

const TZDBNames*
TZDBTimeZoneNames::getMetaZoneNames(const UnicodeString& mzID, UErrorCode& status) {
    umtx_initOnce(gTZDBNamesMapInitOnce, &initTZDBNamesMap, status);
    if (U_FAILURE(status)) {
        return NULL;
    }

    TZDBNames* tzdbNames = NULL;

    UChar mzIDKey[ZID_KEY_MAX + 1];
    mzID.extract(mzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(status == U_ZERO_ERROR);   // already checked length above
    mzIDKey[mzID.length()] = 0;

    umtx_lock(&gTZDBNamesMapLock);
    {
        void *cacheVal = uhash_get(gTZDBNamesMap, mzIDKey);
        if (cacheVal == NULL) {
            UResourceBundle *zoneStringsRes = ures_openDirect(U_ICUDATA_ZONE, "tzdbNames", &status);
            zoneStringsRes = ures_getByKey(zoneStringsRes, gZoneStrings, zoneStringsRes, &status);
            if (U_SUCCESS(status)) {
                char key[ZID_KEY_MAX + 1];
                mergeTimeZoneKey(mzID, key);
                tzdbNames = TZDBNames::createInstance(zoneStringsRes, key);

                if (tzdbNames == NULL) {
                    cacheVal = (void *)EMPTY;
                } else {
                    cacheVal = tzdbNames;
                }
                // Use the persistent ID as the resource key, so we can
                // avoid duplications.
                // TODO: Is there a more efficient way, like intern() in Java?
                void* newKey = (void*) ZoneMeta::findMetaZoneID(mzID);
                if (newKey != NULL) {
                    uhash_put(gTZDBNamesMap, newKey, cacheVal, &status);
                    if (U_FAILURE(status)) {
                        if (tzdbNames != NULL) {
                            delete tzdbNames;
                            tzdbNames = NULL;
                        }
                    }
                } else {
                    // Should never happen with a valid input
                    if (tzdbNames != NULL) {
                        // It's not possible that we get a valid tzdbNames with unknown ID.
                        // But just in case..
                        delete tzdbNames;
                        tzdbNames = NULL;
                    }
                }
            }
            ures_close(zoneStringsRes);
        } else if (cacheVal != EMPTY) {
            tzdbNames = (TZDBNames *)cacheVal;
        }
    }
    umtx_unlock(&gTZDBNamesMapLock);

    return tzdbNames;
}

U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
