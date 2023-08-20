// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2000-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uparse.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000apr18
*   created by: Markus W. Scherer
*
*   This file provides a parser for files that are delimited by one single
*   character like ';' or TAB. Example: the Unicode Character Properties files
*   like UnicodeData.txt are semicolon-delimited.
*/

#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "cstring.h"
#include "filestrm.h"
#include "uparse.h"
#include "ustr_imp.h"

#include <stdio.h>

U_CAPI const char * U_EXPORT2
u_skipWhitespace(const char *s) {
    while(U_IS_INV_WHITESPACE(*s)) {
        ++s;
    }
    return s;
}

U_CAPI char * U_EXPORT2
u_rtrim(char *s) {
    char *end=uprv_strchr(s, 0);
    while(s<end && U_IS_INV_WHITESPACE(*(end-1))) {
        *--end = 0;
    }
    return end;
}

/*
 * If the string starts with # @missing: then return the pointer to the
 * following non-whitespace character.
 * Otherwise return the original pointer.
 * Unicode 5.0 adds such lines in some data files to document
 * default property values.
 * Poor man's regex for variable amounts of white space.
 */
static const char *
getMissingLimit(const char *s) {
    const char *s0=s;
    if(
        *(s=u_skipWhitespace(s))=='#' &&
        *(s=u_skipWhitespace(s+1))=='@' &&
        0==strncmp((s=u_skipWhitespace(s+1)), "missing", 7) &&
        *(s=u_skipWhitespace(s+7))==':'
    ) {
        return u_skipWhitespace(s+1);
    } else {
        return s0;
    }
}

U_CAPI void U_EXPORT2
u_parseDelimitedFile(const char *filename, char delimiter,
                     char *fields[][2], int32_t fieldCount,
                     UParseLineFn *lineFn, void *context,
                     UErrorCode *pErrorCode) {
    FileStream *file;
    char line[10000];
    char *start, *limit;
    int32_t i, length;

    if(U_FAILURE(*pErrorCode)) {
        return;
    }

    if(fields==nullptr || lineFn==nullptr || fieldCount<=0) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if(filename==nullptr || *filename==0 || (*filename=='-' && filename[1]==0)) {
        filename=nullptr;
        file=T_FileStream_stdin();
    } else {
        file=T_FileStream_open(filename, "r");
    }
    if(file==nullptr) {
        *pErrorCode=U_FILE_ACCESS_ERROR;
        return;
    }

    while(T_FileStream_readLine(file, line, sizeof(line))!=nullptr) {
        /* remove trailing newline characters */
        length=(int32_t)(u_rtrim(line)-line);

        /*
         * detect a line with # @missing:
         * start parsing after that, or else from the beginning of the line
         * set the default warning for @missing lines
         */
        start=(char *)getMissingLimit(line);
        if(start==line) {
            *pErrorCode=U_ZERO_ERROR;
        } else {
            *pErrorCode=U_USING_DEFAULT_WARNING;
        }

        /* skip this line if it is empty or a comment */
        if(*start==0 || *start=='#') {
            continue;
        }

        /* remove in-line comments */
        limit=uprv_strchr(start, '#');
        if(limit!=nullptr) {
            /* get white space before the pound sign */
            while(limit>start && U_IS_INV_WHITESPACE(*(limit-1))) {
                --limit;
            }

            /* truncate the line */
            *limit=0;
        }

        /* skip lines with only whitespace */
        if(u_skipWhitespace(start)[0]==0) {
            continue;
        }

        /* for each field, call the corresponding field function */
        for(i=0; i<fieldCount; ++i) {
            /* set the limit pointer of this field */
            limit=start;
            while(*limit!=delimiter && *limit!=0) {
                ++limit;
            }

            /* set the field start and limit in the fields array */
            fields[i][0]=start;
            fields[i][1]=limit;

            /* set start to the beginning of the next field, if any */
            start=limit;
            if(*start!=0) {
                ++start;
            } else if(i+1<fieldCount) {
                *pErrorCode=U_PARSE_ERROR;
                limit=line+length;
                i=fieldCount;
                break;
            }
        }

        /* too few fields? */
        if(U_FAILURE(*pErrorCode)) {
            break;
        }

        /* call the field function */
        lineFn(context, fields, fieldCount, pErrorCode);
        if(U_FAILURE(*pErrorCode)) {
            break;
        }
    }

    if(filename!=nullptr) {
        T_FileStream_close(file);
    }
}

/*
 * parse a list of code points
 * store them as a UTF-32 string in dest[destCapacity]
 * return the number of code points
 */
U_CAPI int32_t U_EXPORT2
u_parseCodePoints(const char *s,
                  uint32_t *dest, int32_t destCapacity,
                  UErrorCode *pErrorCode) {
    char *end;
    uint32_t value;
    int32_t count;

    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(s==nullptr || destCapacity<0 || (destCapacity>0 && dest==nullptr)) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    count=0;
    for(;;) {
        s=u_skipWhitespace(s);
        if(*s==';' || *s==0) {
            return count;
        }

        /* read one code point */
        value=(uint32_t)uprv_strtoul(s, &end, 16);
        if(end<=s || (!U_IS_INV_WHITESPACE(*end) && *end!=';' && *end!=0) || value>=0x110000) {
            *pErrorCode=U_PARSE_ERROR;
            return 0;
        }

        /* append it to the destination array */
        if(count<destCapacity) {
            dest[count++]=value;
        } else {
            *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
        }

        /* go to the following characters */
        s=end;
    }
}

/*
 * parse a list of code points
 * store them as a string in dest[destCapacity]
 * set the first code point in *pFirst
 * @return The length of the string in numbers of UChars.
 */
U_CAPI int32_t U_EXPORT2
u_parseString(const char *s,
              char16_t *dest, int32_t destCapacity,
              uint32_t *pFirst,
              UErrorCode *pErrorCode) {
    char *end;
    uint32_t value;
    int32_t destLength;

    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(s==nullptr || destCapacity<0 || (destCapacity>0 && dest==nullptr)) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if(pFirst!=nullptr) {
        *pFirst=0xffffffff;
    }

    destLength=0;
    for(;;) {
        s=u_skipWhitespace(s);
        if(*s==';' || *s==0) {
            if(destLength<destCapacity) {
                dest[destLength]=0;
            } else if(destLength==destCapacity) {
                *pErrorCode=U_STRING_NOT_TERMINATED_WARNING;
            } else {
                *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
            }
            return destLength;
        }

        /* read one code point */
        value=(uint32_t)uprv_strtoul(s, &end, 16);
        if(end<=s || (!U_IS_INV_WHITESPACE(*end) && *end!=';' && *end!=0) || value>=0x110000) {
            *pErrorCode=U_PARSE_ERROR;
            return 0;
        }

        /* store the first code point */
        if(pFirst!=nullptr) {
            *pFirst=value;
            pFirst=nullptr;
        }

        /* append it to the destination array */
        if((destLength+U16_LENGTH(value))<=destCapacity) {
            U16_APPEND_UNSAFE(dest, destLength, value);
        } else {
            destLength+=U16_LENGTH(value);
        }

        /* go to the following characters */
        s=end;
    }
}

/* read a range like start or start..end */
U_CAPI int32_t U_EXPORT2
u_parseCodePointRangeAnyTerminator(const char *s,
                                   uint32_t *pStart, uint32_t *pEnd,
                                   const char **terminator,
                                   UErrorCode *pErrorCode) {
    char *end;
    uint32_t value;

    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(s==nullptr || pStart==nullptr || pEnd==nullptr) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    /* read the start code point */
    s=u_skipWhitespace(s);
    value=(uint32_t)uprv_strtoul(s, &end, 16);
    if(end<=s || value>=0x110000) {
        *pErrorCode=U_PARSE_ERROR;
        return 0;
    }
    *pStart=*pEnd=value;

    /* is there a "..end"? */
    s=u_skipWhitespace(end);
    if(*s!='.' || s[1]!='.') {
        *terminator=end;
        return 1;
    }
    s=u_skipWhitespace(s+2);

    /* read the end code point */
    value=(uint32_t)uprv_strtoul(s, &end, 16);
    if(end<=s || value>=0x110000) {
        *pErrorCode=U_PARSE_ERROR;
        return 0;
    }
    *pEnd=value;

    /* is this a valid range? */
    if(value<*pStart) {
        *pErrorCode=U_PARSE_ERROR;
        return 0;
    }

    *terminator=end;
    return value-*pStart+1;
}

U_CAPI int32_t U_EXPORT2
u_parseCodePointRange(const char *s,
                      uint32_t *pStart, uint32_t *pEnd,
                      UErrorCode *pErrorCode) {
    const char *terminator;
    int32_t rangeLength=
        u_parseCodePointRangeAnyTerminator(s, pStart, pEnd, &terminator, pErrorCode);
    if(U_SUCCESS(*pErrorCode)) {
        terminator=u_skipWhitespace(terminator);
        if(*terminator!=';' && *terminator!=0) {
            *pErrorCode=U_PARSE_ERROR;
            return 0;
        }
    }
    return rangeLength;
}

U_CAPI int32_t U_EXPORT2
u_parseUTF8(const char *source, int32_t sLen, char *dest, int32_t destCapacity, UErrorCode *status) {
    const char *read = source;
    int32_t i = 0;
    unsigned int value = 0;
    if(sLen == -1) {
        sLen = (int32_t)strlen(source);
    }
    
    while(read < source+sLen) {
        sscanf(read, "%2x", &value);
        if(i < destCapacity) {
            dest[i] = (char)value;
        }
        i++;
        read += 2;
    }
    return u_terminateChars(dest, destCapacity, i, status);
}
