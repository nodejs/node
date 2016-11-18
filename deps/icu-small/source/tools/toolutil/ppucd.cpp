// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ppucd.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011dec11
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "charstr.h"
#include "cstring.h"
#include "ppucd.h"
#include "uassert.h"
#include "uparse.h"

#include <stdio.h>
#include <string.h>

U_NAMESPACE_BEGIN

PropertyNames::~PropertyNames() {}

int32_t
PropertyNames::getPropertyEnum(const char *name) const {
    return u_getPropertyEnum(name);
}

int32_t
PropertyNames::getPropertyValueEnum(int32_t property, const char *name) const {
    return u_getPropertyValueEnum((UProperty)property, name);
}

UniProps::UniProps()
        : start(U_SENTINEL), end(U_SENTINEL),
          bmg(U_SENTINEL), bpb(U_SENTINEL),
          scf(U_SENTINEL), slc(U_SENTINEL), stc(U_SENTINEL), suc(U_SENTINEL),
          digitValue(-1), numericValue(NULL),
          name(NULL), nameAlias(NULL) {
    memset(binProps, 0, sizeof(binProps));
    memset(intProps, 0, sizeof(intProps));
    memset(age, 0, 4);
}

UniProps::~UniProps() {}

const int32_t PreparsedUCD::kNumLineBuffers;

PreparsedUCD::PreparsedUCD(const char *filename, UErrorCode &errorCode)
        : icuPnames(new PropertyNames()), pnames(icuPnames),
          file(NULL),
          defaultLineIndex(-1), blockLineIndex(-1), lineIndex(0),
          lineNumber(0),
          lineType(NO_LINE),
          fieldLimit(NULL), lineLimit(NULL) {
    if(U_FAILURE(errorCode)) { return; }

    if(filename==NULL || *filename==0 || (*filename=='-' && filename[1]==0)) {
        filename=NULL;
        file=stdin;
    } else {
        file=fopen(filename, "r");
    }
    if(file==NULL) {
        perror("error opening preparsed UCD");
        fprintf(stderr, "error opening preparsed UCD file %s\n", filename ? filename : "\"no file name given\"");
        errorCode=U_FILE_ACCESS_ERROR;
        return;
    }

    memset(ucdVersion, 0, 4);
    lines[0][0]=0;
}

PreparsedUCD::~PreparsedUCD() {
    if(file!=stdin) {
        fclose(file);
    }
    delete icuPnames;
}

// Same order as the LineType values.
static const char *lineTypeStrings[]={
    NULL,
    NULL,
    "ucd",
    "property",
    "binary",
    "value",
    "defaults",
    "block",
    "cp",
    "algnamesrange"
};

PreparsedUCD::LineType
PreparsedUCD::readLine(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NO_LINE; }
    // Select the next available line buffer.
    while(!isLineBufferAvailable(lineIndex)) {
        ++lineIndex;
        if (lineIndex == kNumLineBuffers) {
            lineIndex = 0;
        }
    }
    char *line=lines[lineIndex];
    *line=0;
    lineLimit=fieldLimit=line;
    lineType=NO_LINE;
    char *result=fgets(line, sizeof(lines[0]), file);
    if(result==NULL) {
        if(ferror(file)) {
            perror("error reading preparsed UCD");
            fprintf(stderr, "error reading preparsed UCD before line %ld\n", (long)lineNumber);
            errorCode=U_FILE_ACCESS_ERROR;
        }
        return NO_LINE;
    }
    ++lineNumber;
    if(*line=='#') {
        fieldLimit=strchr(line, 0);
        return lineType=EMPTY_LINE;
    }
    // Remove trailing /r/n.
    char c;
    char *limit=strchr(line, 0);
    while(line<limit && ((c=*(limit-1))=='\n' || c=='\r')) { --limit; }
    // Remove trailing white space.
    while(line<limit && ((c=*(limit-1))==' ' || c=='\t')) { --limit; }
    *limit=0;
    lineLimit=limit;
    if(line==limit) {
        fieldLimit=limit;
        return lineType=EMPTY_LINE;
    }
    // Split by ';'.
    char *semi=line;
    while((semi=strchr(semi, ';'))!=NULL) { *semi++=0; }
    fieldLimit=strchr(line, 0);
    // Determine the line type.
    int32_t type;
    for(type=EMPTY_LINE+1;; ++type) {
        if(type==LINE_TYPE_COUNT) {
            fprintf(stderr,
                    "error in preparsed UCD: unknown line type (first field) '%s' on line %ld\n",
                    line, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
            return NO_LINE;
        }
        if(0==strcmp(line, lineTypeStrings[type])) {
            break;
        }
    }
    lineType=(LineType)type;
    if(lineType==UNICODE_VERSION_LINE && fieldLimit<lineLimit) {
        u_versionFromString(ucdVersion, fieldLimit+1);
    }
    return lineType;
}

const char *
PreparsedUCD::firstField() {
    char *field=lines[lineIndex];
    fieldLimit=strchr(field, 0);
    return field;
}

const char *
PreparsedUCD::nextField() {
    if(fieldLimit==lineLimit) { return NULL; }
    char *field=fieldLimit+1;
    fieldLimit=strchr(field, 0);
    return field;
}

const UniProps *
PreparsedUCD::getProps(UnicodeSet &newValues, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    newValues.clear();
    if(!lineHasPropertyValues()) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    firstField();
    const char *field=nextField();
    if(field==NULL) {
        // No range field after the type.
        fprintf(stderr,
                "error in preparsed UCD: missing default/block/cp range field "
                "(no second field) on line %ld\n",
                (long)lineNumber);
        errorCode=U_PARSE_ERROR;
        return NULL;
    }
    UChar32 start, end;
    if(!parseCodePointRange(field, start, end, errorCode)) { return NULL; }
    UniProps *props;
    switch(lineType) {
    case DEFAULTS_LINE:
        if(defaultLineIndex>=0) {
            fprintf(stderr,
                    "error in preparsed UCD: second line with default properties on line %ld\n",
                    (long)lineNumber);
            errorCode=U_PARSE_ERROR;
            return NULL;
        }
        if(start!=0 || end!=0x10ffff) {
            fprintf(stderr,
                    "error in preparsed UCD: default range must be 0..10FFFF, not '%s' on line %ld\n",
                    field, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
            return NULL;
        }
        props=&defaultProps;
        defaultLineIndex=lineIndex;
        break;
    case BLOCK_LINE:
        blockProps=defaultProps;  // Block inherits default properties.
        props=&blockProps;
        blockLineIndex=lineIndex;
        break;
    case CP_LINE:
        if(blockProps.start<=start && end<=blockProps.end) {
            // Code point range fully inside the last block inherits the block properties.
            cpProps=blockProps;
        } else if(start>blockProps.end || end<blockProps.start) {
            // Code point range fully outside the last block inherits the default properties.
            cpProps=defaultProps;
        } else {
            // Code point range partially overlapping with the last block is illegal.
            fprintf(stderr,
                    "error in preparsed UCD: cp range %s on line %ld only "
                    "partially overlaps with block range %04lX..%04lX\n",
                    field, (long)lineNumber, (long)blockProps.start, (long)blockProps.end);
            errorCode=U_PARSE_ERROR;
            return NULL;
        }
        props=&cpProps;
        break;
    default:
        // Will not occur because of the range check above.
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    props->start=start;
    props->end=end;
    while((field=nextField())!=NULL) {
        if(!parseProperty(*props, field, newValues, errorCode)) { return NULL; }
    }
    return props;
}

static const struct {
    const char *name;
    int32_t prop;
} ppucdProperties[]={
    { "Name_Alias", PPUCD_NAME_ALIAS },
    { "Conditional_Case_Mappings", PPUCD_CONDITIONAL_CASE_MAPPINGS },
    { "Turkic_Case_Folding", PPUCD_TURKIC_CASE_FOLDING }
};

// Returns TRUE for "ok to continue parsing fields".
UBool
PreparsedUCD::parseProperty(UniProps &props, const char *field, UnicodeSet &newValues,
                            UErrorCode &errorCode) {
    CharString pBuffer;
    const char *p=field;
    const char *v=strchr(p, '=');
    int binaryValue;
    if(*p=='-') {
        if(v!=NULL) {
            fprintf(stderr,
                    "error in preparsed UCD: mix of binary-property-no and "
                    "enum-property syntax '%s' on line %ld\n",
                    field, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
            return FALSE;
        }
        binaryValue=0;
        ++p;
    } else if(v==NULL) {
        binaryValue=1;
    } else {
        binaryValue=-1;
        // Copy out the property name rather than modifying the field (writing a NUL).
        pBuffer.append(p, (int32_t)(v-p), errorCode);
        p=pBuffer.data();
        ++v;
    }
    int32_t prop=pnames->getPropertyEnum(p);
    if(prop<0) {
        for(int32_t i=0;; ++i) {
            if(i==UPRV_LENGTHOF(ppucdProperties)) {
                // Ignore unknown property names.
                return TRUE;
            }
            if(0==uprv_stricmp(p, ppucdProperties[i].name)) {
                prop=ppucdProperties[i].prop;
                U_ASSERT(prop>=0);
                break;
            }
        }
    }
    if(prop<UCHAR_BINARY_LIMIT) {
        if(binaryValue>=0) {
            props.binProps[prop]=(UBool)binaryValue;
        } else {
            // No binary value for a binary property.
            fprintf(stderr,
                    "error in preparsed UCD: enum-property syntax '%s' "
                    "for binary property on line %ld\n",
                    field, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
        }
    } else if(binaryValue>=0) {
        // Binary value for a non-binary property.
        fprintf(stderr,
                "error in preparsed UCD: binary-property syntax '%s' "
                "for non-binary property on line %ld\n",
                field, (long)lineNumber);
        errorCode=U_PARSE_ERROR;
    } else if (prop < UCHAR_INT_START) {
        fprintf(stderr,
                "error in preparsed UCD: prop value is invalid: '%d' for line %ld\n",
                prop, (long)lineNumber);
        errorCode=U_PARSE_ERROR;
    } else if(prop<UCHAR_INT_LIMIT) {
        int32_t value=pnames->getPropertyValueEnum(prop, v);
        if(value==UCHAR_INVALID_CODE && prop==UCHAR_CANONICAL_COMBINING_CLASS) {
            // TODO: Make getPropertyValueEnum(UCHAR_CANONICAL_COMBINING_CLASS, v) work.
            char *end;
            unsigned long ccc=uprv_strtoul(v, &end, 10);
            if(v<end && *end==0 && ccc<=254) {
                value=(int32_t)ccc;
            }
        }
        if(value==UCHAR_INVALID_CODE) {
            fprintf(stderr,
                    "error in preparsed UCD: '%s' is not a valid value on line %ld\n",
                    field, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
        } else {
            props.intProps[prop-UCHAR_INT_START]=value;
        }
    } else if(*v=='<') {
        // Do not parse default values like <code point>, just set null values.
        switch(prop) {
        case UCHAR_BIDI_MIRRORING_GLYPH:
            props.bmg=U_SENTINEL;
            break;
        case UCHAR_BIDI_PAIRED_BRACKET:
            props.bpb=U_SENTINEL;
            break;
        case UCHAR_SIMPLE_CASE_FOLDING:
            props.scf=U_SENTINEL;
            break;
        case UCHAR_SIMPLE_LOWERCASE_MAPPING:
            props.slc=U_SENTINEL;
            break;
        case UCHAR_SIMPLE_TITLECASE_MAPPING:
            props.stc=U_SENTINEL;
            break;
        case UCHAR_SIMPLE_UPPERCASE_MAPPING:
            props.suc=U_SENTINEL;
            break;
        case UCHAR_CASE_FOLDING:
            props.cf.remove();
            break;
        case UCHAR_LOWERCASE_MAPPING:
            props.lc.remove();
            break;
        case UCHAR_TITLECASE_MAPPING:
            props.tc.remove();
            break;
        case UCHAR_UPPERCASE_MAPPING:
            props.uc.remove();
            break;
        case UCHAR_SCRIPT_EXTENSIONS:
            props.scx.clear();
            break;
        default:
            fprintf(stderr,
                    "error in preparsed UCD: '%s' is not a valid default value on line %ld\n",
                    field, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
        }
    } else {
        char c;
        switch(prop) {
        case UCHAR_NUMERIC_VALUE:
            props.numericValue=v;
            c=*v;
            if('0'<=c && c<='9' && v[1]==0) {
                props.digitValue=c-'0';
            } else {
                props.digitValue=-1;
            }
            break;
        case UCHAR_NAME:
            props.name=v;
            break;
        case UCHAR_AGE:
            u_versionFromString(props.age, v);  // Writes 0.0.0.0 if v is not numeric.
            break;
        case UCHAR_BIDI_MIRRORING_GLYPH:
            props.bmg=parseCodePoint(v, errorCode);
            break;
        case UCHAR_BIDI_PAIRED_BRACKET:
            props.bpb=parseCodePoint(v, errorCode);
            break;
        case UCHAR_SIMPLE_CASE_FOLDING:
            props.scf=parseCodePoint(v, errorCode);
            break;
        case UCHAR_SIMPLE_LOWERCASE_MAPPING:
            props.slc=parseCodePoint(v, errorCode);
            break;
        case UCHAR_SIMPLE_TITLECASE_MAPPING:
            props.stc=parseCodePoint(v, errorCode);
            break;
        case UCHAR_SIMPLE_UPPERCASE_MAPPING:
            props.suc=parseCodePoint(v, errorCode);
            break;
        case UCHAR_CASE_FOLDING:
            parseString(v, props.cf, errorCode);
            break;
        case UCHAR_LOWERCASE_MAPPING:
            parseString(v, props.lc, errorCode);
            break;
        case UCHAR_TITLECASE_MAPPING:
            parseString(v, props.tc, errorCode);
            break;
        case UCHAR_UPPERCASE_MAPPING:
            parseString(v, props.uc, errorCode);
            break;
        case PPUCD_NAME_ALIAS:
            props.nameAlias=v;
            break;
        case PPUCD_CONDITIONAL_CASE_MAPPINGS:
        case PPUCD_TURKIC_CASE_FOLDING:
            // No need to parse their values: They are hardcoded in the runtime library.
            break;
        case UCHAR_SCRIPT_EXTENSIONS:
            parseScriptExtensions(v, props.scx, errorCode);
            break;
        default:
            // Ignore unhandled properties.
            return TRUE;
        }
    }
    if(U_SUCCESS(errorCode)) {
        newValues.add((UChar32)prop);
        return TRUE;
    } else {
        return FALSE;
    }
}

UBool
PreparsedUCD::getRangeForAlgNames(UChar32 &start, UChar32 &end, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    if(lineType!=ALG_NAMES_RANGE_LINE) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    firstField();
    const char *field=nextField();
    if(field==NULL) {
        // No range field after the type.
        fprintf(stderr,
                "error in preparsed UCD: missing algnamesrange range field "
                "(no second field) on line %ld\n",
                (long)lineNumber);
        errorCode=U_PARSE_ERROR;
        return FALSE;
    }
    return parseCodePointRange(field, start, end, errorCode);
}

UChar32
PreparsedUCD::parseCodePoint(const char *s, UErrorCode &errorCode) {
    char *end;
    uint32_t value=(uint32_t)uprv_strtoul(s, &end, 16);
    if(end<=s || *end!=0 || value>=0x110000) {
        fprintf(stderr,
                "error in preparsed UCD: '%s' is not a valid code point on line %ld\n",
                s, (long)lineNumber);
        errorCode=U_PARSE_ERROR;
        return U_SENTINEL;
    }
    return (UChar32)value;
}

UBool
PreparsedUCD::parseCodePointRange(const char *s, UChar32 &start, UChar32 &end, UErrorCode &errorCode) {
    uint32_t st, e;
    u_parseCodePointRange(s, &st, &e, &errorCode);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr,
                "error in preparsed UCD: '%s' is not a valid code point range on line %ld\n",
                s, (long)lineNumber);
        return FALSE;
    }
    start=(UChar32)st;
    end=(UChar32)e;
    return TRUE;
}

void
PreparsedUCD::parseString(const char *s, UnicodeString &uni, UErrorCode &errorCode) {
    UChar *buffer=uni.getBuffer(-1);
    int32_t length=u_parseString(s, buffer, uni.getCapacity(), NULL, &errorCode);
    if(errorCode==U_BUFFER_OVERFLOW_ERROR) {
        errorCode=U_ZERO_ERROR;
        uni.releaseBuffer(0);
        buffer=uni.getBuffer(length);
        length=u_parseString(s, buffer, uni.getCapacity(), NULL, &errorCode);
    }
    uni.releaseBuffer(length);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr,
                "error in preparsed UCD: '%s' is not a valid Unicode string on line %ld\n",
                s, (long)lineNumber);
    }
}

void
PreparsedUCD::parseScriptExtensions(const char *s, UnicodeSet &scx, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    scx.clear();
    CharString scString;
    for(;;) {
        const char *scs;
        const char *scLimit=strchr(s, ' ');
        if(scLimit!=NULL) {
            scs=scString.clear().append(s, (int32_t)(scLimit-s), errorCode).data();
            if(U_FAILURE(errorCode)) { return; }
        } else {
            scs=s;
        }
        int32_t script=pnames->getPropertyValueEnum(UCHAR_SCRIPT, scs);
        if(script==UCHAR_INVALID_CODE) {
            fprintf(stderr,
                    "error in preparsed UCD: '%s' is not a valid script code on line %ld\n",
                    scs, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
            return;
        } else if(scx.contains(script)) {
            fprintf(stderr,
                    "error in preparsed UCD: scx has duplicate '%s' codes on line %ld\n",
                    scs, (long)lineNumber);
            errorCode=U_PARSE_ERROR;
            return;
        } else {
            scx.add(script);
        }
        if(scLimit!=NULL) {
            s=scLimit+1;
        } else {
            break;
        }
    }
    if(scx.isEmpty()) {
        fprintf(stderr, "error in preparsed UCD: empty scx= on line %ld\n", (long)lineNumber);
        errorCode=U_PARSE_ERROR;
    }
}

U_NAMESPACE_END
