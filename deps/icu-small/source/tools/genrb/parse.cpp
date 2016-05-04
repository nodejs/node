/*
*******************************************************************************
*
*   Copyright (C) 1998-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File parse.cpp
*
* Modification History:
*
*   Date          Name          Description
*   05/26/99     stephen       Creation.
*   02/25/00     weiv          Overhaul to write udata
*   5/10/01      Ram           removed ustdio dependency
*   06/10/2001  Dominic Ludlam <dom@recoil.org> Rewritten
*******************************************************************************
*/

// Safer use of UnicodeString.
#ifndef UNISTR_FROM_CHAR_EXPLICIT
#   define UNISTR_FROM_CHAR_EXPLICIT explicit
#endif

// Less important, but still a good idea.
#ifndef UNISTR_FROM_STRING_EXPLICIT
#   define UNISTR_FROM_STRING_EXPLICIT explicit
#endif

#include <assert.h>
#include "parse.h"
#include "errmsg.h"
#include "uhash.h"
#include "cmemory.h"
#include "cstring.h"
#include "uinvchar.h"
#include "read.h"
#include "ustr.h"
#include "reslist.h"
#include "rbt_pars.h"
#include "genrb.h"
#include "unicode/stringpiece.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "unicode/uscript.h"
#include "unicode/utf16.h"
#include "unicode/putil.h"
#include "charstr.h"
#include "collationbuilder.h"
#include "collationdata.h"
#include "collationdatareader.h"
#include "collationdatawriter.h"
#include "collationfastlatinbuilder.h"
#include "collationinfo.h"
#include "collationroot.h"
#include "collationruleparser.h"
#include "collationtailoring.h"
#include <stdio.h>

/* Number of tokens to read ahead of the current stream position */
#define MAX_LOOKAHEAD   3

#define CR               0x000D
#define LF               0x000A
#define SPACE            0x0020
#define TAB              0x0009
#define ESCAPE           0x005C
#define HASH             0x0023
#define QUOTE            0x0027
#define ZERO             0x0030
#define STARTCOMMAND     0x005B
#define ENDCOMMAND       0x005D
#define OPENSQBRACKET    0x005B
#define CLOSESQBRACKET   0x005D

using icu::CharString;
using icu::LocalMemory;
using icu::LocalPointer;
using icu::LocalUCHARBUFPointer;
using icu::StringPiece;
using icu::UnicodeString;

struct Lookahead
{
     enum   ETokenType type;
     struct UString    value;
     struct UString    comment;
     uint32_t          line;
};

/* keep in sync with token defines in read.h */
const char *tokenNames[TOK_TOKEN_COUNT] =
{
     "string",             /* A string token, such as "MonthNames" */
     "'{'",                 /* An opening brace character */
     "'}'",                 /* A closing brace character */
     "','",                 /* A comma */
     "':'",                 /* A colon */

     "<end of file>",     /* End of the file has been reached successfully */
     "<end of line>"
};

/* Just to store "TRUE" */
//static const UChar trueValue[] = {0x0054, 0x0052, 0x0055, 0x0045, 0x0000};

typedef struct {
    struct Lookahead  lookahead[MAX_LOOKAHEAD + 1];
    uint32_t          lookaheadPosition;
    UCHARBUF         *buffer;
    struct SRBRoot *bundle;
    const char     *inputdir;
    uint32_t        inputdirLength;
    const char     *outputdir;
    uint32_t        outputdirLength;
    const char     *filename;
    UBool           makeBinaryCollation;
    UBool           omitCollationRules;
} ParseState;

typedef struct SResource *
ParseResourceFunction(ParseState* state, char *tag, uint32_t startline, const struct UString* comment, UErrorCode *status);

static struct SResource *parseResource(ParseState* state, char *tag, const struct UString *comment, UErrorCode *status);

/* The nature of the lookahead buffer:
   There are MAX_LOOKAHEAD + 1 slots, used as a circular buffer.  This provides
   MAX_LOOKAHEAD lookahead tokens and a slot for the current token and value.
   When getToken is called, the current pointer is moved to the next slot and the
   old slot is filled with the next token from the reader by calling getNextToken.
   The token values are stored in the slot, which means that token values don't
   survive a call to getToken, ie.

   UString *value;

   getToken(&value, NULL, status);
   getToken(NULL,   NULL, status);       bad - value is now a different string
*/
static void
initLookahead(ParseState* state, UCHARBUF *buf, UErrorCode *status)
{
    static uint32_t initTypeStrings = 0;
    uint32_t i;

    if (!initTypeStrings)
    {
        initTypeStrings = 1;
    }

    state->lookaheadPosition   = 0;
    state->buffer              = buf;

    resetLineNumber();

    for (i = 0; i < MAX_LOOKAHEAD; i++)
    {
        state->lookahead[i].type = getNextToken(state->buffer, &state->lookahead[i].value, &state->lookahead[i].line, &state->lookahead[i].comment, status);
        if (U_FAILURE(*status))
        {
            return;
        }
    }

    *status = U_ZERO_ERROR;
}

static void
cleanupLookahead(ParseState* state)
{
    uint32_t i;
    for (i = 0; i <= MAX_LOOKAHEAD; i++)
    {
        ustr_deinit(&state->lookahead[i].value);
        ustr_deinit(&state->lookahead[i].comment);
    }

}

static enum ETokenType
getToken(ParseState* state, struct UString **tokenValue, struct UString* comment, uint32_t *linenumber, UErrorCode *status)
{
    enum ETokenType result;
    uint32_t          i;

    result = state->lookahead[state->lookaheadPosition].type;

    if (tokenValue != NULL)
    {
        *tokenValue = &state->lookahead[state->lookaheadPosition].value;
    }

    if (linenumber != NULL)
    {
        *linenumber = state->lookahead[state->lookaheadPosition].line;
    }

    if (comment != NULL)
    {
        ustr_cpy(comment, &(state->lookahead[state->lookaheadPosition].comment), status);
    }

    i = (state->lookaheadPosition + MAX_LOOKAHEAD) % (MAX_LOOKAHEAD + 1);
    state->lookaheadPosition = (state->lookaheadPosition + 1) % (MAX_LOOKAHEAD + 1);
    ustr_setlen(&state->lookahead[i].comment, 0, status);
    ustr_setlen(&state->lookahead[i].value, 0, status);
    state->lookahead[i].type = getNextToken(state->buffer, &state->lookahead[i].value, &state->lookahead[i].line, &state->lookahead[i].comment, status);

    /* printf("getToken, returning %s\n", tokenNames[result]); */

    return result;
}

static enum ETokenType
peekToken(ParseState* state, uint32_t lookaheadCount, struct UString **tokenValue, uint32_t *linenumber, struct UString *comment, UErrorCode *status)
{
    uint32_t i = (state->lookaheadPosition + lookaheadCount) % (MAX_LOOKAHEAD + 1);

    if (U_FAILURE(*status))
    {
        return TOK_ERROR;
    }

    if (lookaheadCount >= MAX_LOOKAHEAD)
    {
        *status = U_INTERNAL_PROGRAM_ERROR;
        return TOK_ERROR;
    }

    if (tokenValue != NULL)
    {
        *tokenValue = &state->lookahead[i].value;
    }

    if (linenumber != NULL)
    {
        *linenumber = state->lookahead[i].line;
    }

    if(comment != NULL){
        ustr_cpy(comment, &(state->lookahead[state->lookaheadPosition].comment), status);
    }

    return state->lookahead[i].type;
}

static void
expect(ParseState* state, enum ETokenType expectedToken, struct UString **tokenValue, struct UString *comment, uint32_t *linenumber, UErrorCode *status)
{
    uint32_t        line;

    enum ETokenType token = getToken(state, tokenValue, comment, &line, status);

    if (linenumber != NULL)
    {
        *linenumber = line;
    }

    if (U_FAILURE(*status))
    {
        return;
    }

    if (token != expectedToken)
    {
        *status = U_INVALID_FORMAT_ERROR;
        error(line, "expecting %s, got %s", tokenNames[expectedToken], tokenNames[token]);
    }
    else
    {
        *status = U_ZERO_ERROR;
    }
}

static char *getInvariantString(ParseState* state, uint32_t *line, struct UString *comment, UErrorCode *status)
{
    struct UString *tokenValue;
    char           *result;
    uint32_t        count;

    expect(state, TOK_STRING, &tokenValue, comment, line, status);

    if (U_FAILURE(*status))
    {
        return NULL;
    }

    count = u_strlen(tokenValue->fChars);
    if(!uprv_isInvariantUString(tokenValue->fChars, count)) {
        *status = U_INVALID_FORMAT_ERROR;
        error(*line, "invariant characters required for table keys, binary data, etc.");
        return NULL;
    }

    result = static_cast<char *>(uprv_malloc(count+1));

    if (result == NULL)
    {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }

    u_UCharsToChars(tokenValue->fChars, result, count+1);
    return result;
}

static struct SResource *
parseUCARules(ParseState* state, char *tag, uint32_t startline, const struct UString* /*comment*/, UErrorCode *status)
{
    struct SResource *result = NULL;
    struct UString   *tokenValue;
    FileStream       *file          = NULL;
    char              filename[256] = { '\0' };
    char              cs[128]       = { '\0' };
    uint32_t          line;
    UBool quoted = FALSE;
    UCHARBUF *ucbuf=NULL;
    UChar32   c     = 0;
    const char* cp  = NULL;
    UChar *pTarget     = NULL;
    UChar *target      = NULL;
    UChar *targetLimit = NULL;
    int32_t size = 0;

    expect(state, TOK_STRING, &tokenValue, NULL, &line, status);

    if(isVerbose()){
        printf(" %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    if (U_FAILURE(*status))
    {
        return NULL;
    }
    /* make the filename including the directory */
    if (state->inputdir != NULL)
    {
        uprv_strcat(filename, state->inputdir);

        if (state->inputdir[state->inputdirLength - 1] != U_FILE_SEP_CHAR)
        {
            uprv_strcat(filename, U_FILE_SEP_STRING);
        }
    }

    u_UCharsToChars(tokenValue->fChars, cs, tokenValue->fLength);

    expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);

    if (U_FAILURE(*status))
    {
        return NULL;
    }
    uprv_strcat(filename, cs);

    if(state->omitCollationRules) {
        return res_none();
    }

    ucbuf = ucbuf_open(filename, &cp, getShowWarning(),FALSE, status);

    if (U_FAILURE(*status)) {
        error(line, "An error occured while opening the input file %s\n", filename);
        return NULL;
    }

    /* We allocate more space than actually required
    * since the actual size needed for storing UChars
    * is not known in UTF-8 byte stream
    */
    size        = ucbuf_size(ucbuf) + 1;
    pTarget     = (UChar*) uprv_malloc(U_SIZEOF_UCHAR * size);
    uprv_memset(pTarget, 0, size*U_SIZEOF_UCHAR);
    target      = pTarget;
    targetLimit = pTarget+size;

    /* read the rules into the buffer */
    while (target < targetLimit)
    {
        c = ucbuf_getc(ucbuf, status);
        if(c == QUOTE) {
            quoted = (UBool)!quoted;
        }
        /* weiv (06/26/2002): adding the following:
         * - preserving spaces in commands [...]
         * - # comments until the end of line
         */
        if (c == STARTCOMMAND && !quoted)
        {
            /* preserve commands
             * closing bracket will be handled by the
             * append at the end of the loop
             */
            while(c != ENDCOMMAND) {
                U_APPEND_CHAR32_ONLY(c, target);
                c = ucbuf_getc(ucbuf, status);
            }
        }
        else if (c == HASH && !quoted) {
            /* skip comments */
            while(c != CR && c != LF) {
                c = ucbuf_getc(ucbuf, status);
            }
            continue;
        }
        else if (c == ESCAPE)
        {
            c = unescape(ucbuf, status);

            if (c == (UChar32)U_ERR)
            {
                uprv_free(pTarget);
                T_FileStream_close(file);
                return NULL;
            }
        }
        else if (!quoted && (c == SPACE || c == TAB || c == CR || c == LF))
        {
            /* ignore spaces carriage returns
            * and line feed unless in the form \uXXXX
            */
            continue;
        }

        /* Append UChar * after dissembling if c > 0xffff*/
        if (c != (UChar32)U_EOF)
        {
            U_APPEND_CHAR32_ONLY(c, target);
        }
        else
        {
            break;
        }
    }

    /* terminate the string */
    if(target < targetLimit){
        *target = 0x0000;
    }

    result = string_open(state->bundle, tag, pTarget, (int32_t)(target - pTarget), NULL, status);


    ucbuf_close(ucbuf);
    uprv_free(pTarget);
    T_FileStream_close(file);

    return result;
}

static struct SResource *
parseTransliterator(ParseState* state, char *tag, uint32_t startline, const struct UString* /*comment*/, UErrorCode *status)
{
    struct SResource *result = NULL;
    struct UString   *tokenValue;
    FileStream       *file          = NULL;
    char              filename[256] = { '\0' };
    char              cs[128]       = { '\0' };
    uint32_t          line;
    UCHARBUF *ucbuf=NULL;
    const char* cp  = NULL;
    UChar *pTarget     = NULL;
    const UChar *pSource     = NULL;
    int32_t size = 0;

    expect(state, TOK_STRING, &tokenValue, NULL, &line, status);

    if(isVerbose()){
        printf(" %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    if (U_FAILURE(*status))
    {
        return NULL;
    }
    /* make the filename including the directory */
    if (state->inputdir != NULL)
    {
        uprv_strcat(filename, state->inputdir);

        if (state->inputdir[state->inputdirLength - 1] != U_FILE_SEP_CHAR)
        {
            uprv_strcat(filename, U_FILE_SEP_STRING);
        }
    }

    u_UCharsToChars(tokenValue->fChars, cs, tokenValue->fLength);

    expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);

    if (U_FAILURE(*status))
    {
        return NULL;
    }
    uprv_strcat(filename, cs);


    ucbuf = ucbuf_open(filename, &cp, getShowWarning(),FALSE, status);

    if (U_FAILURE(*status)) {
        error(line, "An error occured while opening the input file %s\n", filename);
        return NULL;
    }

    /* We allocate more space than actually required
    * since the actual size needed for storing UChars
    * is not known in UTF-8 byte stream
    */
    pSource = ucbuf_getBuffer(ucbuf, &size, status);
    pTarget     = (UChar*) uprv_malloc(U_SIZEOF_UCHAR * (size + 1));
    uprv_memset(pTarget, 0, size*U_SIZEOF_UCHAR);

#if !UCONFIG_NO_TRANSLITERATION
    size = utrans_stripRules(pSource, size, pTarget, status);
#else
    size = 0;
    fprintf(stderr, " Warning: writing empty transliteration data ( UCONFIG_NO_TRANSLITERATION ) \n");
#endif
    result = string_open(state->bundle, tag, pTarget, size, NULL, status);

    ucbuf_close(ucbuf);
    uprv_free(pTarget);
    T_FileStream_close(file);

    return result;
}
static ArrayResource* dependencyArray = NULL;

static struct SResource *
parseDependency(ParseState* state, char *tag, uint32_t startline, const struct UString* comment, UErrorCode *status)
{
    struct SResource *result = NULL;
    struct SResource *elem = NULL;
    struct UString   *tokenValue;
    uint32_t          line;
    char              filename[256] = { '\0' };
    char              cs[128]       = { '\0' };
    
    expect(state, TOK_STRING, &tokenValue, NULL, &line, status);

    if(isVerbose()){
        printf(" %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    if (U_FAILURE(*status))
    {
        return NULL;
    }
    /* make the filename including the directory */
    if (state->outputdir != NULL)
    {
        uprv_strcat(filename, state->outputdir);

        if (state->outputdir[state->outputdirLength - 1] != U_FILE_SEP_CHAR)
        {
            uprv_strcat(filename, U_FILE_SEP_STRING);
        }
    }
    
    u_UCharsToChars(tokenValue->fChars, cs, tokenValue->fLength);

    if (U_FAILURE(*status))
    {
        return NULL;
    }
    uprv_strcat(filename, cs);
    if(!T_FileStream_file_exists(filename)){
        if(isStrict()){
            error(line, "The dependency file %s does not exist. Please make sure it exists.\n",filename);
        }else{
            warning(line, "The dependency file %s does not exist. Please make sure it exists.\n",filename);       
        }
    }
    if(dependencyArray==NULL){
        dependencyArray = array_open(state->bundle, "%%DEPENDENCY", NULL, status);
    }
    if(tag!=NULL){
        result = string_open(state->bundle, tag, tokenValue->fChars, tokenValue->fLength, comment, status);
    }
    elem = string_open(state->bundle, NULL, tokenValue->fChars, tokenValue->fLength, comment, status);

    dependencyArray->add(elem);

    if (U_FAILURE(*status))
    {
        return NULL;
    }
    expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);
    return result;
}
static struct SResource *
parseString(ParseState* state, char *tag, uint32_t startline, const struct UString* comment, UErrorCode *status)
{
    struct UString   *tokenValue;
    struct SResource *result = NULL;

/*    if (tag != NULL && uprv_strcmp(tag, "%%UCARULES") == 0)
    {
        return parseUCARules(tag, startline, status);
    }*/
    if(isVerbose()){
        printf(" string %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }
    expect(state, TOK_STRING, &tokenValue, NULL, NULL, status);

    if (U_SUCCESS(*status))
    {
        /* create the string now - tokenValue doesn't survive a call to getToken (and therefore
        doesn't survive expect either) */

        result = string_open(state->bundle, tag, tokenValue->fChars, tokenValue->fLength, comment, status);
        if(U_SUCCESS(*status) && result) {
            expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);

            if (U_FAILURE(*status))
            {
                res_close(result);
                return NULL;
            }
        }
    }

    return result;
}

static struct SResource *
parseAlias(ParseState* state, char *tag, uint32_t startline, const struct UString *comment, UErrorCode *status)
{
    struct UString   *tokenValue;
    struct SResource *result  = NULL;

    expect(state, TOK_STRING, &tokenValue, NULL, NULL, status);

    if(isVerbose()){
        printf(" alias %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    if (U_SUCCESS(*status))
    {
        /* create the string now - tokenValue doesn't survive a call to getToken (and therefore
        doesn't survive expect either) */

        result = alias_open(state->bundle, tag, tokenValue->fChars, tokenValue->fLength, comment, status);

        expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);

        if (U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }
    }

    return result;
}

#if !UCONFIG_NO_COLLATION

namespace {

static struct SResource* resLookup(struct SResource* res, const char* key){
    if (res == res_none() || !res->isTable()) {
        return NULL;
    }

    TableResource *list = static_cast<TableResource *>(res);
    SResource *current = list->fFirst;
    while (current != NULL) {
        if (uprv_strcmp(((list->fRoot->fKeys) + (current->fKey)), key) == 0) {
            return current;
        }
        current = current->fNext;
    }
    return NULL;
}

class GenrbImporter : public icu::CollationRuleParser::Importer {
public:
    GenrbImporter(const char *in, const char *out) : inputDir(in), outputDir(out) {}
    virtual ~GenrbImporter();
    virtual void getRules(
            const char *localeID, const char *collationType,
            UnicodeString &rules,
            const char *&errorReason, UErrorCode &errorCode);

private:
    const char *inputDir;
    const char *outputDir;
};

GenrbImporter::~GenrbImporter() {}

void
GenrbImporter::getRules(
        const char *localeID, const char *collationType,
        UnicodeString &rules,
        const char *& /*errorReason*/, UErrorCode &errorCode) {
    CharString filename(localeID, errorCode);
    for(int32_t i = 0; i < filename.length(); i++){
        if(filename[i] == '-'){
            filename.data()[i] = '_';
        }
    }
    filename.append(".txt", errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }
    CharString inputDirBuf;
    CharString openFileName;
    if(inputDir == NULL) {
        const char *filenameBegin = uprv_strrchr(filename.data(), U_FILE_SEP_CHAR);
        if (filenameBegin != NULL) {
            /*
             * When a filename ../../../data/root.txt is specified,
             * we presume that the input directory is ../../../data
             * This is very important when the resource file includes
             * another file, like UCARules.txt or thaidict.brk.
             */
            StringPiece dir = filename.toStringPiece();
            const char *filenameLimit = filename.data() + filename.length();
            dir.remove_suffix((int32_t)(filenameLimit - filenameBegin));
            inputDirBuf.append(dir, errorCode);
            inputDir = inputDirBuf.data();
        }
    }else{
        int32_t dirlen  = (int32_t)uprv_strlen(inputDir);

        if((filename[0] != U_FILE_SEP_CHAR) && (inputDir[dirlen-1] !='.')) {
            /*
             * append the input dir to openFileName if the first char in
             * filename is not file separator char and the last char input directory is  not '.'.
             * This is to support :
             * genrb -s. /home/icu/data
             * genrb -s. icu/data
             * The user cannot mix notations like
             * genrb -s. /icu/data --- the absolute path specified. -s redundant
             * user should use
             * genrb -s. icu/data  --- start from CWD and look in icu/data dir
             */
            openFileName.append(inputDir, dirlen, errorCode);
            if(inputDir[dirlen-1] != U_FILE_SEP_CHAR) {
                openFileName.append(U_FILE_SEP_CHAR, errorCode);
            }
        }
    }
    openFileName.append(filename, errorCode);
    if(U_FAILURE(errorCode)) {
        return;
    }
    // printf("GenrbImporter::getRules(%s, %s) reads %s\n", localeID, collationType, openFileName.data());
    const char* cp = "";
    LocalUCHARBUFPointer ucbuf(
            ucbuf_open(openFileName.data(), &cp, getShowWarning(), TRUE, &errorCode));
    if(errorCode == U_FILE_ACCESS_ERROR) {
        fprintf(stderr, "couldn't open file %s\n", openFileName.data());
        return;
    }
    if (ucbuf.isNull() || U_FAILURE(errorCode)) {
        fprintf(stderr, "An error occured processing file %s. Error: %s\n", openFileName.data(), u_errorName(errorCode));
        return;
    }

    /* Parse the data into an SRBRoot */
    struct SRBRoot *data =
            parse(ucbuf.getAlias(), inputDir, outputDir, filename.data(), FALSE, FALSE, &errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }

    struct SResource *root = data->fRoot;
    struct SResource *collations = resLookup(root, "collations");
    if (collations != NULL) {
      struct SResource *collation = resLookup(collations, collationType);
      if (collation != NULL) {
        struct SResource *sequence = resLookup(collation, "Sequence");
        if (sequence != NULL && sequence->isString()) {
          // No string pointer aliasing so that we need not hold onto the resource bundle.
          StringResource *sr = static_cast<StringResource *>(sequence);
          rules = sr->fString;
        }
      }
    }
}

// Quick-and-dirty escaping function.
// Assumes that we are on an ASCII-based platform.
static void
escape(const UChar *s, char *buffer) {
    int32_t length = u_strlen(s);
    int32_t i = 0;
    for (;;) {
        UChar32 c;
        U16_NEXT(s, i, length, c);
        if (c == 0) {
            *buffer = 0;
            return;
        } else if (0x20 <= c && c <= 0x7e) {
            // printable ASCII
            *buffer++ = (char)c;  // assumes ASCII-based platform
        } else {
            buffer += sprintf(buffer, "\\u%04X", (int)c);
        }
    }
}

}  // namespace

#endif  // !UCONFIG_NO_COLLATION

static TableResource *
addCollation(ParseState* state, TableResource  *result, const char *collationType,
             uint32_t startline, UErrorCode *status)
{
    // TODO: Use LocalPointer for result, or make caller close it when there is a failure.
    struct SResource  *member = NULL;
    struct UString    *tokenValue;
    struct UString     comment;
    enum   ETokenType  token;
    char               subtag[1024];
    UnicodeString      rules;
    UBool              haveRules = FALSE;
    UVersionInfo       version;
    uint32_t           line;

    /* '{' . (name resource)* '}' */
    version[0]=0; version[1]=0; version[2]=0; version[3]=0;

    for (;;)
    {
        ustr_init(&comment);
        token = getToken(state, &tokenValue, &comment, &line, status);

        if (token == TOK_CLOSE_BRACE)
        {
            break;
        }

        if (token != TOK_STRING)
        {
            res_close(result);
            *status = U_INVALID_FORMAT_ERROR;

            if (token == TOK_EOF)
            {
                error(startline, "unterminated table");
            }
            else
            {
                error(line, "Unexpected token %s", tokenNames[token]);
            }

            return NULL;
        }

        u_UCharsToChars(tokenValue->fChars, subtag, u_strlen(tokenValue->fChars) + 1);

        if (U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }

        member = parseResource(state, subtag, NULL, status);

        if (U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }
        if (result == NULL)
        {
            // Ignore the parsed resources, continue parsing.
        }
        else if (uprv_strcmp(subtag, "Version") == 0 && member->isString())
        {
            StringResource *sr = static_cast<StringResource *>(member);
            char     ver[40];
            int32_t length = sr->length();

            if (length >= UPRV_LENGTHOF(ver))
            {
                length = UPRV_LENGTHOF(ver) - 1;
            }

            sr->fString.extract(0, length, ver, UPRV_LENGTHOF(ver), US_INV);
            u_versionFromString(version, ver);

            result->add(member, line, *status);
            member = NULL;
        }
        else if(uprv_strcmp(subtag, "%%CollationBin")==0)
        {
            /* discard duplicate %%CollationBin if any*/
        }
        else if (uprv_strcmp(subtag, "Sequence") == 0 && member->isString())
        {
            StringResource *sr = static_cast<StringResource *>(member);
            rules = sr->fString;
            haveRules = TRUE;
            // Defer building the collator until we have seen
            // all sub-elements of the collation table, including the Version.
            /* in order to achieve smaller data files, we can direct genrb */
            /* to omit collation rules */
            if(!state->omitCollationRules) {
                result->add(member, line, *status);
                member = NULL;
            }
        }
        else  // Just copy non-special items.
        {
            result->add(member, line, *status);
            member = NULL;
        }
        res_close(member);  // TODO: use LocalPointer
        if (U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }
    }

    if (!haveRules) { return result; }

#if UCONFIG_NO_COLLATION || UCONFIG_NO_FILE_IO
    warning(line, "Not building collation elements because of UCONFIG_NO_COLLATION and/or UCONFIG_NO_FILE_IO, see uconfig.h");
    (void)collationType;
#else
    // CLDR ticket #3949, ICU ticket #8082:
    // Do not build collation binary data for for-import-only "private" collation rule strings.
    if (uprv_strncmp(collationType, "private-", 8) == 0) {
        if(isVerbose()) {
            printf("Not building %s~%s collation binary\n", state->filename, collationType);
        }
        return result;
    }

    if(!state->makeBinaryCollation) {
        if(isVerbose()) {
            printf("Not building %s~%s collation binary\n", state->filename, collationType);
        }
        return result;
    }
    UErrorCode intStatus = U_ZERO_ERROR;
    UParseError parseError;
    uprv_memset(&parseError, 0, sizeof(parseError));
    GenrbImporter importer(state->inputdir, state->outputdir);
    const icu::CollationTailoring *base = icu::CollationRoot::getRoot(intStatus);
    if(U_FAILURE(intStatus)) {
        error(line, "failed to load root collator (ucadata.icu) - %s", u_errorName(intStatus));
        res_close(result);
        return NULL;  // TODO: use LocalUResourceBundlePointer for result
    }
    icu::CollationBuilder builder(base, intStatus);
    if(uprv_strncmp(collationType, "search", 6) == 0) {
        builder.disableFastLatin();  // build fast-Latin table unless search collator
    }
    LocalPointer<icu::CollationTailoring> t(
            builder.parseAndBuild(rules, version, &importer, &parseError, intStatus));
    if(U_FAILURE(intStatus)) {
        const char *reason = builder.getErrorReason();
        if(reason == NULL) { reason = ""; }
        error(line, "CollationBuilder failed at %s~%s/Sequence rule offset %ld: %s  %s",
                state->filename, collationType,
                (long)parseError.offset, u_errorName(intStatus), reason);
        if(parseError.preContext[0] != 0 || parseError.postContext[0] != 0) {
            // Print pre- and post-context.
            char preBuffer[100], postBuffer[100];
            escape(parseError.preContext, preBuffer);
            escape(parseError.postContext, postBuffer);
            error(line, "  error context: \"...%s\" ! \"%s...\"", preBuffer, postBuffer);
        }
        if(isStrict() || t.isNull()) {
            *status = intStatus;
            res_close(result);
            return NULL;
        }
    }
    icu::LocalMemory<uint8_t> buffer;
    int32_t capacity = 100000;
    uint8_t *dest = buffer.allocateInsteadAndCopy(capacity);
    if(dest == NULL) {
        fprintf(stderr, "memory allocation (%ld bytes) for file contents failed\n",
                (long)capacity);
        *status = U_MEMORY_ALLOCATION_ERROR;
        res_close(result);
        return NULL;
    }
    int32_t indexes[icu::CollationDataReader::IX_TOTAL_SIZE + 1];
    int32_t totalSize = icu::CollationDataWriter::writeTailoring(
            *t, *t->settings, indexes, dest, capacity, intStatus);
    if(intStatus == U_BUFFER_OVERFLOW_ERROR) {
        intStatus = U_ZERO_ERROR;
        capacity = totalSize;
        dest = buffer.allocateInsteadAndCopy(capacity);
        if(dest == NULL) {
            fprintf(stderr, "memory allocation (%ld bytes) for file contents failed\n",
                    (long)capacity);
            *status = U_MEMORY_ALLOCATION_ERROR;
            res_close(result);
            return NULL;
        }
        totalSize = icu::CollationDataWriter::writeTailoring(
                *t, *t->settings, indexes, dest, capacity, intStatus);
    }
    if(U_FAILURE(intStatus)) {
        fprintf(stderr, "CollationDataWriter::writeTailoring() failed: %s\n",
                u_errorName(intStatus));
        res_close(result);
        return NULL;
    }
    if(isVerbose()) {
        printf("%s~%s collation tailoring part sizes:\n", state->filename, collationType);
        icu::CollationInfo::printSizes(totalSize, indexes);
        if(t->settings->hasReordering()) {
            printf("%s~%s collation reordering ranges:\n", state->filename, collationType);
            icu::CollationInfo::printReorderRanges(
                    *t->data, t->settings->reorderCodes, t->settings->reorderCodesLength);
        }
    }
    struct SResource *collationBin = bin_open(state->bundle, "%%CollationBin", totalSize, dest, NULL, NULL, status);
    result->add(collationBin, line, *status);
    if (U_FAILURE(*status)) {
        res_close(result);
        return NULL;
    }
#endif
    return result;
}

static UBool
keepCollationType(const char * /*type*/) {
    return TRUE;
}

static struct SResource *
parseCollationElements(ParseState* state, char *tag, uint32_t startline, UBool newCollation, UErrorCode *status)
{
    TableResource  *result = NULL;
    struct SResource  *member = NULL;
    struct UString    *tokenValue;
    struct UString     comment;
    enum   ETokenType  token;
    char               subtag[1024], typeKeyword[1024];
    uint32_t           line;

    result = table_open(state->bundle, tag, NULL, status);

    if (result == NULL || U_FAILURE(*status))
    {
        return NULL;
    }
    if(isVerbose()){
        printf(" collation elements %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }
    if(!newCollation) {
        return addCollation(state, result, "(no type)", startline, status);
    }
    else {
        for(;;) {
            ustr_init(&comment);
            token = getToken(state, &tokenValue, &comment, &line, status);

            if (token == TOK_CLOSE_BRACE)
            {
                return result;
            }

            if (token != TOK_STRING)
            {
                res_close(result);
                *status = U_INVALID_FORMAT_ERROR;

                if (token == TOK_EOF)
                {
                    error(startline, "unterminated table");
                }
                else
                {
                    error(line, "Unexpected token %s", tokenNames[token]);
                }

                return NULL;
            }

            u_UCharsToChars(tokenValue->fChars, subtag, u_strlen(tokenValue->fChars) + 1);

            if (U_FAILURE(*status))
            {
                res_close(result);
                return NULL;
            }

            if (uprv_strcmp(subtag, "default") == 0)
            {
                member = parseResource(state, subtag, NULL, status);

                if (U_FAILURE(*status))
                {
                    res_close(result);
                    return NULL;
                }

                result->add(member, line, *status);
            }
            else
            {
                token = peekToken(state, 0, &tokenValue, &line, &comment, status);
                /* this probably needs to be refactored or recursively use the parser */
                /* first we assume that our collation table won't have the explicit type */
                /* then, we cannot handle aliases */
                if(token == TOK_OPEN_BRACE) {
                    token = getToken(state, &tokenValue, &comment, &line, status);
                    TableResource *collationRes;
                    if (keepCollationType(subtag)) {
                        collationRes = table_open(state->bundle, subtag, NULL, status);
                    } else {
                        collationRes = NULL;
                    }
                    // need to parse the collation data regardless
                    collationRes = addCollation(state, collationRes, subtag, startline, status);
                    if (collationRes != NULL) {
                        result->add(collationRes, startline, *status);
                    }
                } else if(token == TOK_COLON) { /* right now, we'll just try to see if we have aliases */
                    /* we could have a table too */
                    token = peekToken(state, 1, &tokenValue, &line, &comment, status);
                    u_UCharsToChars(tokenValue->fChars, typeKeyword, u_strlen(tokenValue->fChars) + 1);
                    if(uprv_strcmp(typeKeyword, "alias") == 0) {
                        member = parseResource(state, subtag, NULL, status);
                        if (U_FAILURE(*status))
                        {
                            res_close(result);
                            return NULL;
                        }

                        result->add(member, line, *status);
                    } else {
                        res_close(result);
                        *status = U_INVALID_FORMAT_ERROR;
                        return NULL;
                    }
                } else {
                    res_close(result);
                    *status = U_INVALID_FORMAT_ERROR;
                    return NULL;
                }
            }

            /*member = string_open(bundle, subtag, tokenValue->fChars, tokenValue->fLength, status);*/

            /*expect(TOK_CLOSE_BRACE, NULL, NULL, status);*/

            if (U_FAILURE(*status))
            {
                res_close(result);
                return NULL;
            }
        }
    }
}

/* Necessary, because CollationElements requires the bundle->fRoot member to be present which,
   if this weren't special-cased, wouldn't be set until the entire file had been processed. */
static struct SResource *
realParseTable(ParseState* state, TableResource *table, char *tag, uint32_t startline, UErrorCode *status)
{
    struct SResource  *member = NULL;
    struct UString    *tokenValue=NULL;
    struct UString    comment;
    enum   ETokenType token;
    char              subtag[1024];
    uint32_t          line;
    UBool             readToken = FALSE;

    /* '{' . (name resource)* '}' */

    if(isVerbose()){
        printf(" parsing table %s at line %i \n", (tag == NULL) ? "(null)" : tag, (int)startline);
    }
    for (;;)
    {
        ustr_init(&comment);
        token = getToken(state, &tokenValue, &comment, &line, status);

        if (token == TOK_CLOSE_BRACE)
        {
            if (!readToken) {
                warning(startline, "Encountered empty table");
            }
            return table;
        }

        if (token != TOK_STRING)
        {
            *status = U_INVALID_FORMAT_ERROR;

            if (token == TOK_EOF)
            {
                error(startline, "unterminated table");
            }
            else
            {
                error(line, "unexpected token %s", tokenNames[token]);
            }

            return NULL;
        }

        if(uprv_isInvariantUString(tokenValue->fChars, -1)) {
            u_UCharsToChars(tokenValue->fChars, subtag, u_strlen(tokenValue->fChars) + 1);
        } else {
            *status = U_INVALID_FORMAT_ERROR;
            error(line, "invariant characters required for table keys");
            return NULL;
        }

        if (U_FAILURE(*status))
        {
            error(line, "parse error. Stopped parsing tokens with %s", u_errorName(*status));
            return NULL;
        }

        member = parseResource(state, subtag, &comment, status);

        if (member == NULL || U_FAILURE(*status))
        {
            error(line, "parse error. Stopped parsing resource with %s", u_errorName(*status));
            return NULL;
        }

        table->add(member, line, *status);

        if (U_FAILURE(*status))
        {
            error(line, "parse error. Stopped parsing table with %s", u_errorName(*status));
            return NULL;
        }
        readToken = TRUE;
        ustr_deinit(&comment);
   }

    /* not reached */
    /* A compiler warning will appear if all paths don't contain a return statement. */
/*     *status = U_INTERNAL_PROGRAM_ERROR;
     return NULL;*/
}

static struct SResource *
parseTable(ParseState* state, char *tag, uint32_t startline, const struct UString *comment, UErrorCode *status)
{
    if (tag != NULL && uprv_strcmp(tag, "CollationElements") == 0)
    {
        return parseCollationElements(state, tag, startline, FALSE, status);
    }
    if (tag != NULL && uprv_strcmp(tag, "collations") == 0)
    {
        return parseCollationElements(state, tag, startline, TRUE, status);
    }
    if(isVerbose()){
        printf(" table %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    TableResource *result = table_open(state->bundle, tag, comment, status);

    if (result == NULL || U_FAILURE(*status))
    {
        return NULL;
    }
    return realParseTable(state, result, tag, startline,  status);
}

static struct SResource *
parseArray(ParseState* state, char *tag, uint32_t startline, const struct UString *comment, UErrorCode *status)
{
    struct SResource  *member = NULL;
    struct UString    *tokenValue;
    struct UString    memberComments;
    enum   ETokenType token;
    UBool             readToken = FALSE;

    ArrayResource  *result = array_open(state->bundle, tag, comment, status);

    if (result == NULL || U_FAILURE(*status))
    {
        return NULL;
    }
    if(isVerbose()){
        printf(" array %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    ustr_init(&memberComments);

    /* '{' . resource [','] '}' */
    for (;;)
    {
        /* reset length */
        ustr_setlen(&memberComments, 0, status);

        /* check for end of array, but don't consume next token unless it really is the end */
        token = peekToken(state, 0, &tokenValue, NULL, &memberComments, status);


        if (token == TOK_CLOSE_BRACE)
        {
            getToken(state, NULL, NULL, NULL, status);
            if (!readToken) {
                warning(startline, "Encountered empty array");
            }
            break;
        }

        if (token == TOK_EOF)
        {
            res_close(result);
            *status = U_INVALID_FORMAT_ERROR;
            error(startline, "unterminated array");
            return NULL;
        }

        /* string arrays are a special case */
        if (token == TOK_STRING)
        {
            getToken(state, &tokenValue, &memberComments, NULL, status);
            member = string_open(state->bundle, NULL, tokenValue->fChars, tokenValue->fLength, &memberComments, status);
        }
        else
        {
            member = parseResource(state, NULL, &memberComments, status);
        }

        if (member == NULL || U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }

        result->add(member);

        /* eat optional comma if present */
        token = peekToken(state, 0, NULL, NULL, NULL, status);

        if (token == TOK_COMMA)
        {
            getToken(state, NULL, NULL, NULL, status);
        }

        if (U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }
        readToken = TRUE;
    }

    ustr_deinit(&memberComments);
    return result;
}

static struct SResource *
parseIntVector(ParseState* state, char *tag, uint32_t startline, const struct UString *comment, UErrorCode *status)
{
    enum   ETokenType  token;
    char              *string;
    int32_t            value;
    UBool              readToken = FALSE;
    char              *stopstring;
    uint32_t           len;
    struct UString     memberComments;

    IntVectorResource *result = intvector_open(state->bundle, tag, comment, status);

    if (result == NULL || U_FAILURE(*status))
    {
        return NULL;
    }

    if(isVerbose()){
        printf(" vector %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }
    ustr_init(&memberComments);
    /* '{' . string [','] '}' */
    for (;;)
    {
        ustr_setlen(&memberComments, 0, status);

        /* check for end of array, but don't consume next token unless it really is the end */
        token = peekToken(state, 0, NULL, NULL,&memberComments, status);

        if (token == TOK_CLOSE_BRACE)
        {
            /* it's the end, consume the close brace */
            getToken(state, NULL, NULL, NULL, status);
            if (!readToken) {
                warning(startline, "Encountered empty int vector");
            }
            ustr_deinit(&memberComments);
            return result;
        }

        string = getInvariantString(state, NULL, NULL, status);

        if (U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }

        /* For handling illegal char in the Intvector */
        value = uprv_strtoul(string, &stopstring, 0);/* make intvector support decimal,hexdigit,octal digit ranging from -2^31-2^32-1*/
        len=(uint32_t)(stopstring-string);

        if(len==uprv_strlen(string))
        {
            result->add(value, *status);
            uprv_free(string);
            token = peekToken(state, 0, NULL, NULL, NULL, status);
        }
        else
        {
            uprv_free(string);
            *status=U_INVALID_CHAR_FOUND;
        }

        if (U_FAILURE(*status))
        {
            res_close(result);
            return NULL;
        }

        /* the comma is optional (even though it is required to prevent the reader from concatenating
        consecutive entries) so that a missing comma on the last entry isn't an error */
        if (token == TOK_COMMA)
        {
            getToken(state, NULL, NULL, NULL, status);
        }
        readToken = TRUE;
    }

    /* not reached */
    /* A compiler warning will appear if all paths don't contain a return statement. */
/*    intvector_close(result, status);
    *status = U_INTERNAL_PROGRAM_ERROR;
    return NULL;*/
}

static struct SResource *
parseBinary(ParseState* state, char *tag, uint32_t startline, const struct UString *comment, UErrorCode *status)
{
    uint32_t line;
    LocalMemory<char> string(getInvariantString(state, &line, NULL, status));
    if (string.isNull() || U_FAILURE(*status))
    {
        return NULL;
    }

    expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);
    if (U_FAILURE(*status))
    {
        return NULL;
    }

    if(isVerbose()){
        printf(" binary %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    uint32_t count = (uint32_t)uprv_strlen(string.getAlias());
    if (count > 0){
        if((count % 2)==0){
            LocalMemory<uint8_t> value;
            if (value.allocateInsteadAndCopy(count) == NULL)
            {
                *status = U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }

            char toConv[3] = {'\0', '\0', '\0'};
            for (uint32_t i = 0; i < count; i += 2)
            {
                toConv[0] = string[i];
                toConv[1] = string[i + 1];

                char *stopstring;
                value[i >> 1] = (uint8_t) uprv_strtoul(toConv, &stopstring, 16);
                uint32_t len=(uint32_t)(stopstring-toConv);

                if(len!=2)
                {
                    *status=U_INVALID_CHAR_FOUND;
                    return NULL;
                }
            }

            return bin_open(state->bundle, tag, count >> 1, value.getAlias(), NULL, comment, status);
        }
        else
        {
            *status = U_INVALID_CHAR_FOUND;
            error(line, "Encountered invalid binary value (length is odd)");
            return NULL;
        }
    }
    else
    {
        warning(startline, "Encountered empty binary value");
        return bin_open(state->bundle, tag, 0, NULL, "", comment, status);
    }
}

static struct SResource *
parseInteger(ParseState* state, char *tag, uint32_t startline, const struct UString *comment, UErrorCode *status)
{
    struct SResource *result = NULL;
    int32_t           value;
    char             *string;
    char             *stopstring;
    uint32_t          len;

    string = getInvariantString(state, NULL, NULL, status);

    if (string == NULL || U_FAILURE(*status))
    {
        return NULL;
    }

    expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);

    if (U_FAILURE(*status))
    {
        uprv_free(string);
        return NULL;
    }

    if(isVerbose()){
        printf(" integer %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    if (uprv_strlen(string) <= 0)
    {
        warning(startline, "Encountered empty integer. Default value is 0.");
    }

    /* Allow integer support for hexdecimal, octal digit and decimal*/
    /* and handle illegal char in the integer*/
    value = uprv_strtoul(string, &stopstring, 0);
    len=(uint32_t)(stopstring-string);
    if(len==uprv_strlen(string))
    {
        result = int_open(state->bundle, tag, value, comment, status);
    }
    else
    {
        *status=U_INVALID_CHAR_FOUND;
    }
    uprv_free(string);

    return result;
}

static struct SResource *
parseImport(ParseState* state, char *tag, uint32_t startline, const struct UString* comment, UErrorCode *status)
{
    uint32_t          line;
    LocalMemory<char> filename(getInvariantString(state, &line, NULL, status));
    if (U_FAILURE(*status))
    {
        return NULL;
    }

    expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);

    if (U_FAILURE(*status))
    {
        return NULL;
    }

    if(isVerbose()){
        printf(" import %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    /* Open the input file for reading */
    CharString fullname;
    if (state->inputdir != NULL) {
        fullname.append(state->inputdir, *status);
    }
    fullname.appendPathPart(filename.getAlias(), *status);
    if (U_FAILURE(*status)) {
        return NULL;
    }

    FileStream *file = T_FileStream_open(fullname.data(), "rb");
    if (file == NULL)
    {
        error(line, "couldn't open input file %s", filename.getAlias());
        *status = U_FILE_ACCESS_ERROR;
        return NULL;
    }

    int32_t len  = T_FileStream_size(file);
    LocalMemory<uint8_t> data;
    if(data.allocateInsteadAndCopy(len) == NULL)
    {
        *status = U_MEMORY_ALLOCATION_ERROR;
        T_FileStream_close (file);
        return NULL;
    }

    /* int32_t numRead = */ T_FileStream_read(file, data.getAlias(), len);
    T_FileStream_close (file);

    return bin_open(state->bundle, tag, len, data.getAlias(), fullname.data(), comment, status);
}

static struct SResource *
parseInclude(ParseState* state, char *tag, uint32_t startline, const struct UString* comment, UErrorCode *status)
{
    struct SResource *result;
    int32_t           len=0;
    char             *filename;
    uint32_t          line;
    UChar *pTarget     = NULL;

    UCHARBUF *ucbuf;
    char     *fullname = NULL;
    int32_t  count     = 0;
    const char* cp = NULL;
    const UChar* uBuffer = NULL;

    filename = getInvariantString(state, &line, NULL, status);
    count     = (int32_t)uprv_strlen(filename);

    if (U_FAILURE(*status))
    {
        return NULL;
    }

    expect(state, TOK_CLOSE_BRACE, NULL, NULL, NULL, status);

    if (U_FAILURE(*status))
    {
        uprv_free(filename);
        return NULL;
    }

    if(isVerbose()){
        printf(" include %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    fullname = (char *) uprv_malloc(state->inputdirLength + count + 2);
    /* test for NULL */
    if(fullname == NULL)
    {
        *status = U_MEMORY_ALLOCATION_ERROR;
        uprv_free(filename);
        return NULL;
    }

    if(state->inputdir!=NULL){
        if (state->inputdir[state->inputdirLength - 1] != U_FILE_SEP_CHAR)
        {

            uprv_strcpy(fullname, state->inputdir);

            fullname[state->inputdirLength]      = U_FILE_SEP_CHAR;
            fullname[state->inputdirLength + 1] = '\0';

            uprv_strcat(fullname, filename);
        }
        else
        {
            uprv_strcpy(fullname, state->inputdir);
            uprv_strcat(fullname, filename);
        }
    }else{
        uprv_strcpy(fullname,filename);
    }

    ucbuf = ucbuf_open(fullname, &cp,getShowWarning(),FALSE,status);

    if (U_FAILURE(*status)) {
        error(line, "couldn't open input file %s\n", filename);
        return NULL;
    }

    uBuffer = ucbuf_getBuffer(ucbuf,&len,status);
    result = string_open(state->bundle, tag, uBuffer, len, comment, status);

    ucbuf_close(ucbuf);

    uprv_free(pTarget);

    uprv_free(filename);
    uprv_free(fullname);

    return result;
}





U_STRING_DECL(k_type_string,    "string",    6);
U_STRING_DECL(k_type_binary,    "binary",    6);
U_STRING_DECL(k_type_bin,       "bin",       3);
U_STRING_DECL(k_type_table,     "table",     5);
U_STRING_DECL(k_type_table_no_fallback,     "table(nofallback)",         17);
U_STRING_DECL(k_type_int,       "int",       3);
U_STRING_DECL(k_type_integer,   "integer",   7);
U_STRING_DECL(k_type_array,     "array",     5);
U_STRING_DECL(k_type_alias,     "alias",     5);
U_STRING_DECL(k_type_intvector, "intvector", 9);
U_STRING_DECL(k_type_import,    "import",    6);
U_STRING_DECL(k_type_include,   "include",   7);

/* Various non-standard processing plugins that create one or more special resources. */
U_STRING_DECL(k_type_plugin_uca_rules,      "process(uca_rules)",        18);
U_STRING_DECL(k_type_plugin_collation,      "process(collation)",        18);
U_STRING_DECL(k_type_plugin_transliterator, "process(transliterator)",   23);
U_STRING_DECL(k_type_plugin_dependency,     "process(dependency)",       19);

typedef enum EResourceType
{
    RESTYPE_UNKNOWN,
    RESTYPE_STRING,
    RESTYPE_BINARY,
    RESTYPE_TABLE,
    RESTYPE_TABLE_NO_FALLBACK,
    RESTYPE_INTEGER,
    RESTYPE_ARRAY,
    RESTYPE_ALIAS,
    RESTYPE_INTVECTOR,
    RESTYPE_IMPORT,
    RESTYPE_INCLUDE,
    RESTYPE_PROCESS_UCA_RULES,
    RESTYPE_PROCESS_COLLATION,
    RESTYPE_PROCESS_TRANSLITERATOR,
    RESTYPE_PROCESS_DEPENDENCY,
    RESTYPE_RESERVED
} EResourceType;

static struct {
    const char *nameChars;   /* only used for debugging */
    const UChar *nameUChars;
    ParseResourceFunction *parseFunction;
} gResourceTypes[] = {
    {"Unknown", NULL, NULL},
    {"string", k_type_string, parseString},
    {"binary", k_type_binary, parseBinary},
    {"table", k_type_table, parseTable},
    {"table(nofallback)", k_type_table_no_fallback, NULL}, /* parseFunction will never be called */
    {"integer", k_type_integer, parseInteger},
    {"array", k_type_array, parseArray},
    {"alias", k_type_alias, parseAlias},
    {"intvector", k_type_intvector, parseIntVector},
    {"import", k_type_import, parseImport},
    {"include", k_type_include, parseInclude},
    {"process(uca_rules)", k_type_plugin_uca_rules, parseUCARules},
    {"process(collation)", k_type_plugin_collation, NULL /* not implemented yet */},
    {"process(transliterator)", k_type_plugin_transliterator, parseTransliterator},
    {"process(dependency)", k_type_plugin_dependency, parseDependency},
    {"reserved", NULL, NULL}
};

void initParser()
{
    U_STRING_INIT(k_type_string,    "string",    6);
    U_STRING_INIT(k_type_binary,    "binary",    6);
    U_STRING_INIT(k_type_bin,       "bin",       3);
    U_STRING_INIT(k_type_table,     "table",     5);
    U_STRING_INIT(k_type_table_no_fallback,     "table(nofallback)",         17);
    U_STRING_INIT(k_type_int,       "int",       3);
    U_STRING_INIT(k_type_integer,   "integer",   7);
    U_STRING_INIT(k_type_array,     "array",     5);
    U_STRING_INIT(k_type_alias,     "alias",     5);
    U_STRING_INIT(k_type_intvector, "intvector", 9);
    U_STRING_INIT(k_type_import,    "import",    6);
    U_STRING_INIT(k_type_include,   "include",   7);

    U_STRING_INIT(k_type_plugin_uca_rules,      "process(uca_rules)",        18);
    U_STRING_INIT(k_type_plugin_collation,      "process(collation)",        18);
    U_STRING_INIT(k_type_plugin_transliterator, "process(transliterator)",   23);
    U_STRING_INIT(k_type_plugin_dependency,     "process(dependency)",       19);
}

static inline UBool isTable(enum EResourceType type) {
    return (UBool)(type==RESTYPE_TABLE || type==RESTYPE_TABLE_NO_FALLBACK);
}

static enum EResourceType
parseResourceType(ParseState* state, UErrorCode *status)
{
    struct UString        *tokenValue;
    struct UString        comment;
    enum   EResourceType  result = RESTYPE_UNKNOWN;
    uint32_t              line=0;
    ustr_init(&comment);
    expect(state, TOK_STRING, &tokenValue, &comment, &line, status);

    if (U_FAILURE(*status))
    {
        return RESTYPE_UNKNOWN;
    }

    *status = U_ZERO_ERROR;

    /* Search for normal types */
    result=RESTYPE_UNKNOWN;
    while ((result=(EResourceType)(result+1)) < RESTYPE_RESERVED) {
        if (u_strcmp(tokenValue->fChars, gResourceTypes[result].nameUChars) == 0) {
            break;
        }
    }
    /* Now search for the aliases */
    if (u_strcmp(tokenValue->fChars, k_type_int) == 0) {
        result = RESTYPE_INTEGER;
    }
    else if (u_strcmp(tokenValue->fChars, k_type_bin) == 0) {
        result = RESTYPE_BINARY;
    }
    else if (result == RESTYPE_RESERVED) {
        char tokenBuffer[1024];
        u_austrncpy(tokenBuffer, tokenValue->fChars, sizeof(tokenBuffer));
        tokenBuffer[sizeof(tokenBuffer) - 1] = 0;
        *status = U_INVALID_FORMAT_ERROR;
        error(line, "unknown resource type '%s'", tokenBuffer);
    }

    return result;
}

/* parse a non-top-level resource */
static struct SResource *
parseResource(ParseState* state, char *tag, const struct UString *comment, UErrorCode *status)
{
    enum   ETokenType      token;
    enum   EResourceType  resType = RESTYPE_UNKNOWN;
    ParseResourceFunction *parseFunction = NULL;
    struct UString        *tokenValue;
    uint32_t                 startline;
    uint32_t                 line;


    token = getToken(state, &tokenValue, NULL, &startline, status);

    if(isVerbose()){
        printf(" resource %s at line %i \n",  (tag == NULL) ? "(null)" : tag, (int)startline);
    }

    /* name . [ ':' type ] '{' resource '}' */
    /* This function parses from the colon onwards.  If the colon is present, parse the
    type then try to parse a resource of that type.  If there is no explicit type,
    work it out using the lookahead tokens. */
    switch (token)
    {
    case TOK_EOF:
        *status = U_INVALID_FORMAT_ERROR;
        error(startline, "Unexpected EOF encountered");
        return NULL;

    case TOK_ERROR:
        *status = U_INVALID_FORMAT_ERROR;
        return NULL;

    case TOK_COLON:
        resType = parseResourceType(state, status);
        expect(state, TOK_OPEN_BRACE, &tokenValue, NULL, &startline, status);

        if (U_FAILURE(*status))
        {
            return NULL;
        }

        break;

    case TOK_OPEN_BRACE:
        break;

    default:
        *status = U_INVALID_FORMAT_ERROR;
        error(startline, "syntax error while reading a resource, expected '{' or ':'");
        return NULL;
    }


    if (resType == RESTYPE_UNKNOWN)
    {
        /* No explicit type, so try to work it out.  At this point, we've read the first '{'.
        We could have any of the following:
        { {         => array (nested)
        { :/}       => array
        { string ,  => string array

        { string {  => table

        { string :/{    => table
        { string }      => string
        */

        token = peekToken(state, 0, NULL, &line, NULL,status);

        if (U_FAILURE(*status))
        {
            return NULL;
        }

        if (token == TOK_OPEN_BRACE || token == TOK_COLON ||token ==TOK_CLOSE_BRACE )
        {
            resType = RESTYPE_ARRAY;
        }
        else if (token == TOK_STRING)
        {
            token = peekToken(state, 1, NULL, &line, NULL, status);

            if (U_FAILURE(*status))
            {
                return NULL;
            }

            switch (token)
            {
            case TOK_COMMA:         resType = RESTYPE_ARRAY;  break;
            case TOK_OPEN_BRACE:    resType = RESTYPE_TABLE;  break;
            case TOK_CLOSE_BRACE:   resType = RESTYPE_STRING; break;
            case TOK_COLON:         resType = RESTYPE_TABLE;  break;
            default:
                *status = U_INVALID_FORMAT_ERROR;
                error(line, "Unexpected token after string, expected ',', '{' or '}'");
                return NULL;
            }
        }
        else
        {
            *status = U_INVALID_FORMAT_ERROR;
            error(line, "Unexpected token after '{'");
            return NULL;
        }

        /* printf("Type guessed as %s\n", resourceNames[resType]); */
    } else if(resType == RESTYPE_TABLE_NO_FALLBACK) {
        *status = U_INVALID_FORMAT_ERROR;
        error(startline, "error: %s resource type not valid except on top bundle level", gResourceTypes[resType].nameChars);
        return NULL;
    }


    /* We should now know what we need to parse next, so call the appropriate parser
    function and return. */
    parseFunction = gResourceTypes[resType].parseFunction;
    if (parseFunction != NULL) {
        return parseFunction(state, tag, startline, comment, status);
    }
    else {
        *status = U_INTERNAL_PROGRAM_ERROR;
        error(startline, "internal error: %s resource type found and not handled", gResourceTypes[resType].nameChars);
    }

    return NULL;
}

/* parse the top-level resource */
struct SRBRoot *
parse(UCHARBUF *buf, const char *inputDir, const char *outputDir, const char *filename,
      UBool makeBinaryCollation, UBool omitCollationRules, UErrorCode *status)
{
    struct UString    *tokenValue;
    struct UString    comment;
    uint32_t           line;
    enum EResourceType bundleType;
    enum ETokenType    token;
    ParseState state;
    uint32_t i;


    for (i = 0; i < MAX_LOOKAHEAD + 1; i++)
    {
        ustr_init(&state.lookahead[i].value);
        ustr_init(&state.lookahead[i].comment);
    }

    initLookahead(&state, buf, status);

    state.inputdir       = inputDir;
    state.inputdirLength = (state.inputdir != NULL) ? (uint32_t)uprv_strlen(state.inputdir) : 0;
    state.outputdir       = outputDir;
    state.outputdirLength = (state.outputdir != NULL) ? (uint32_t)uprv_strlen(state.outputdir) : 0;
    state.filename = filename;
    state.makeBinaryCollation = makeBinaryCollation;
    state.omitCollationRules = omitCollationRules;

    ustr_init(&comment);
    expect(&state, TOK_STRING, &tokenValue, &comment, NULL, status);

    state.bundle = new SRBRoot(&comment, FALSE, *status);

    if (state.bundle == NULL || U_FAILURE(*status))
    {
        return NULL;
    }


    state.bundle->setLocale(tokenValue->fChars, *status);

    /* The following code is to make Empty bundle work no matter with :table specifer or not */
    token = getToken(&state, NULL, NULL, &line, status);
    if(token==TOK_COLON) {
        *status=U_ZERO_ERROR;
        bundleType=parseResourceType(&state, status);

        if(isTable(bundleType))
        {
            expect(&state, TOK_OPEN_BRACE, NULL, NULL, &line, status);
        }
        else
        {
            *status=U_PARSE_ERROR;
             error(line, "parse error. Stopped parsing with %s", u_errorName(*status));
        }
    }
    else
    {
        /* not a colon */
        if(token==TOK_OPEN_BRACE)
        {
            *status=U_ZERO_ERROR;
            bundleType=RESTYPE_TABLE;
        }
        else
        {
            /* neither colon nor open brace */
            *status=U_PARSE_ERROR;
            bundleType=RESTYPE_UNKNOWN;
            error(line, "parse error, did not find open-brace '{' or colon ':', stopped with %s", u_errorName(*status));
        }
    }

    if (U_FAILURE(*status))
    {
        delete state.bundle;
        return NULL;
    }

    if(bundleType==RESTYPE_TABLE_NO_FALLBACK) {
        /*
         * Parse a top-level table with the table(nofallback) declaration.
         * This is the same as a regular table, but also sets the
         * URES_ATT_NO_FALLBACK flag in indexes[URES_INDEX_ATTRIBUTES] .
         */
        state.bundle->fNoFallback=TRUE;
    }
    /* top-level tables need not handle special table names like "collations" */
    assert(!state.bundle->fIsPoolBundle);
    assert(state.bundle->fRoot->fType == URES_TABLE);
    TableResource *rootTable = static_cast<TableResource *>(state.bundle->fRoot);
    realParseTable(&state, rootTable, NULL, line, status);
    if(dependencyArray!=NULL){
        rootTable->add(dependencyArray, 0, *status);
        dependencyArray = NULL;
    }
   if (U_FAILURE(*status))
    {
        delete state.bundle;
        res_close(dependencyArray);
        return NULL;
    }

    if (getToken(&state, NULL, NULL, &line, status) != TOK_EOF)
    {
        warning(line, "extraneous text after resource bundle (perhaps unmatched braces)");
        if(isStrict()){
            *status = U_INVALID_FORMAT_ERROR;
            return NULL;
        }
    }

    cleanupLookahead(&state);
    ustr_deinit(&comment);
    return state.bundle;
}
