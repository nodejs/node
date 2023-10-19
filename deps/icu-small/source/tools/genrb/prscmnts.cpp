// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 *   Copyright (C) 2003-2014, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 *
 * File prscmnts.cpp
 *
 * Modification History:
 *
 *   Date          Name        Description
 *   08/22/2003    ram         Creation.
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

#include "unicode/regex.h"
#include "unicode/unistr.h"
#include "unicode/parseerr.h"
#include "prscmnts.h"
#include <stdio.h>
#include <stdlib.h>

U_NAMESPACE_USE

#if UCONFIG_NO_REGULAR_EXPRESSIONS==0 /* donot compile when RegularExpressions not available */

#define MAX_SPLIT_STRINGS 20

const char *patternStrings[UPC_LIMIT]={
    "^translate\\s*(.*)",
    "^note\\s*(.*)"
};

U_CFUNC int32_t 
removeText(char16_t *source, int32_t srcLen,
           UnicodeString patString,uint32_t options,  
           UnicodeString replaceText, UErrorCode *status){

    if(status == nullptr || U_FAILURE(*status)){
        return 0;
    }

    UnicodeString src(source, srcLen);

    RegexMatcher    myMatcher(patString, src, options, *status);
    if(U_FAILURE(*status)){
        return 0;
    }
    UnicodeString dest;


    dest = myMatcher.replaceAll(replaceText,*status);
    
    
    return dest.extract(source, srcLen, *status);

}
U_CFUNC int32_t
trim(char16_t *src, int32_t srcLen, UErrorCode *status){
     srcLen = removeText(src, srcLen, UnicodeString("^[ \\r\\n]+ "), 0, UnicodeString(), status); // remove leading new lines
     srcLen = removeText(src, srcLen, UnicodeString("^\\s+"), 0, UnicodeString(), status); // remove leading spaces
     srcLen = removeText(src, srcLen, UnicodeString("\\s+$"), 0, UnicodeString(), status); // remove trailing spcaes
     return srcLen;
}

U_CFUNC int32_t 
removeCmtText(char16_t* source, int32_t srcLen, UErrorCode* status){
    srcLen = trim(source, srcLen, status);
    UnicodeString patString("^\\s*?\\*\\s*?");  // remove pattern like " * " at the beginning of the line
    srcLen = removeText(source, srcLen, patString, UREGEX_MULTILINE, UnicodeString(), status);
    return removeText(source, srcLen, UnicodeString("[ \\r\\n]+"), 0, UnicodeString(" "), status);// remove new lines;
}

U_CFUNC int32_t 
getText(const char16_t* source, int32_t srcLen,
        char16_t** dest, int32_t destCapacity,
        UnicodeString patternString, 
        UErrorCode* status){
    
    if(status == nullptr || U_FAILURE(*status)){
        return 0;
    }

    UnicodeString     stringArray[MAX_SPLIT_STRINGS];
    RegexPattern      *pattern = RegexPattern::compile(UnicodeString("@"), 0, *status);
    UnicodeString src (source,srcLen);
    
    if (U_FAILURE(*status)) {
        return 0;
    }
    pattern->split(src, stringArray, MAX_SPLIT_STRINGS, *status);
    
    RegexMatcher matcher(patternString, UREGEX_DOTALL, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    for(int32_t i=0; i<MAX_SPLIT_STRINGS; i++){
        matcher.reset(stringArray[i]);
        if(matcher.lookingAt(*status)){
            UnicodeString out = matcher.group(1, *status);

            return out.extract(*dest, destCapacity,*status);
        }
    }
    return 0;
}


#define AT_SIGN  0x0040

U_CFUNC int32_t
getDescription( const char16_t* source, int32_t srcLen,
                char16_t** dest, int32_t destCapacity,
                UErrorCode* status){
    if(status == nullptr || U_FAILURE(*status)){
        return 0;
    }

    UnicodeString     stringArray[MAX_SPLIT_STRINGS];
    RegexPattern      *pattern = RegexPattern::compile(UnicodeString("@"), UREGEX_MULTILINE, *status);
    UnicodeString src(source, srcLen);
    
    if (U_FAILURE(*status)) {
        return 0;
    }
    pattern->split(src, stringArray,MAX_SPLIT_STRINGS , *status);

    if(stringArray[0].indexOf((char16_t)AT_SIGN)==-1){
        int32_t destLen =  stringArray[0].extract(*dest, destCapacity, *status);
        return trim(*dest, destLen, status);
    }
    return 0;
}

U_CFUNC int32_t
getCount(const char16_t* source, int32_t srcLen,
         UParseCommentsOption option, UErrorCode *status){
    
    if(status == nullptr || U_FAILURE(*status)){
        return 0;
    }

    UnicodeString     stringArray[MAX_SPLIT_STRINGS];
    RegexPattern      *pattern = RegexPattern::compile(UnicodeString("@"), UREGEX_MULTILINE, *status);
    UnicodeString src (source, srcLen);


    if (U_FAILURE(*status)) {
        return 0;
    }
    int32_t retLen = pattern->split(src, stringArray, MAX_SPLIT_STRINGS, *status);
    
    UnicodeString patternString(patternStrings[option]);
    RegexMatcher matcher(patternString, UREGEX_DOTALL, *status);
    if (U_FAILURE(*status)) {
        return 0;
    } 
    int32_t count = 0;
    for(int32_t i=0; i<retLen; i++){
        matcher.reset(stringArray[i]);
        if(matcher.lookingAt(*status)){
            count++;
        }
    }
    if(option == UPC_TRANSLATE && count > 1){
        fprintf(stderr, "Multiple @translate tags cannot be supported.\n");
        exit(U_UNSUPPORTED_ERROR);
    }
    return count;
}

U_CFUNC int32_t 
getAt(const char16_t* source, int32_t srcLen,
        char16_t** dest, int32_t destCapacity,
        int32_t index,
        UParseCommentsOption option,
        UErrorCode* status){

    if(status == nullptr || U_FAILURE(*status)){
        return 0;
    }

    UnicodeString     stringArray[MAX_SPLIT_STRINGS];
    RegexPattern      *pattern = RegexPattern::compile(UnicodeString("@"), UREGEX_MULTILINE, *status);
    UnicodeString src (source, srcLen);


    if (U_FAILURE(*status)) {
        return 0;
    }
    int32_t retLen = pattern->split(src, stringArray, MAX_SPLIT_STRINGS, *status);
    
    UnicodeString patternString(patternStrings[option]);
    RegexMatcher matcher(patternString, UREGEX_DOTALL, *status);
    if (U_FAILURE(*status)) {
        return 0;
    } 
    int32_t count = 0;
    for(int32_t i=0; i<retLen; i++){
        matcher.reset(stringArray[i]);
        if(matcher.lookingAt(*status)){
            if(count == index){
                UnicodeString out = matcher.group(1, *status);
                return out.extract(*dest, destCapacity,*status);
            }
            count++;
            
        }
    }
    return 0;

}

U_CFUNC int32_t
getTranslate( const char16_t* source, int32_t srcLen,
              char16_t** dest, int32_t destCapacity,
              UErrorCode* status){
    UnicodeString     notePatternString("^translate\\s*?(.*)");
    
    int32_t destLen = getText(source, srcLen, dest, destCapacity, notePatternString, status);
    return trim(*dest, destLen, status);
}

U_CFUNC int32_t 
getNote(const char16_t* source, int32_t srcLen,
        char16_t** dest, int32_t destCapacity,
        UErrorCode* status){

    UnicodeString     notePatternString("^note\\s*?(.*)");
    int32_t destLen =  getText(source, srcLen, dest, destCapacity, notePatternString, status);
    return trim(*dest, destLen, status);

}

#endif /* UCONFIG_NO_REGULAR_EXPRESSIONS */

