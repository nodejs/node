/*
*******************************************************************************
*
*   Copyright (C) 1998-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File uscnnf_p.c
*
* Modification History:
*
*   Date        Name        Description
*   12/02/98    stephen        Creation.
*   03/13/99    stephen     Modified for new C API.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_CONVERSION

#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/unum.h"
#include "unicode/udat.h"
#include "unicode/uset.h"
#include "uscanf.h"
#include "ufmt_cmn.h"
#include "ufile.h"
#include "locbund.h"

#include "cmemory.h"
#include "ustr_cnv.h"

/* flag characters for u_scanf */
#define FLAG_ASTERISK 0x002A
#define FLAG_PAREN 0x0028

#define ISFLAG(s)    (s) == FLAG_ASTERISK || \
            (s) == FLAG_PAREN

/* special characters for u_scanf */
#define SPEC_DOLLARSIGN 0x0024

/* unicode digits */
#define DIGIT_ZERO 0x0030
#define DIGIT_ONE 0x0031
#define DIGIT_TWO 0x0032
#define DIGIT_THREE 0x0033
#define DIGIT_FOUR 0x0034
#define DIGIT_FIVE 0x0035
#define DIGIT_SIX 0x0036
#define DIGIT_SEVEN 0x0037
#define DIGIT_EIGHT 0x0038
#define DIGIT_NINE 0x0039

#define ISDIGIT(s)    (s) == DIGIT_ZERO || \
            (s) == DIGIT_ONE || \
            (s) == DIGIT_TWO || \
            (s) == DIGIT_THREE || \
            (s) == DIGIT_FOUR || \
            (s) == DIGIT_FIVE || \
            (s) == DIGIT_SIX || \
            (s) == DIGIT_SEVEN || \
            (s) == DIGIT_EIGHT || \
            (s) == DIGIT_NINE

/* u_scanf modifiers */
#define MOD_H 0x0068
#define MOD_LOWERL 0x006C
#define MOD_L 0x004C

#define ISMOD(s)    (s) == MOD_H || \
            (s) == MOD_LOWERL || \
            (s) == MOD_L

/**
 * Struct encapsulating a single uscanf format specification.
 */
typedef struct u_scanf_spec_info {
    int32_t fWidth;         /* Width  */

    UChar   fSpec;          /* Format specification  */

    UChar   fPadChar;       /* Padding character  */

    UBool   fSkipArg;       /* TRUE if arg should be skipped */
    UBool   fIsLongDouble;  /* L flag  */
    UBool   fIsShort;       /* h flag  */
    UBool   fIsLong;        /* l flag  */
    UBool   fIsLongLong;    /* ll flag  */
    UBool   fIsString;      /* TRUE if this is a NULL-terminated string. */
} u_scanf_spec_info;


/**
 * Struct encapsulating a single u_scanf format specification.
 */
typedef struct u_scanf_spec {
    u_scanf_spec_info    fInfo;        /* Information on this spec */
    int32_t        fArgPos;    /* Position of data in arg list */
} u_scanf_spec;

/**
 * Parse a single u_scanf format specifier in Unicode.
 * @param fmt A pointer to a '%' character in a u_scanf format specification.
 * @param spec A pointer to a <TT>u_scanf_spec</TT> to receive the parsed
 * format specifier.
 * @return The number of characters contained in this specifier.
 */
static int32_t
u_scanf_parse_spec (const UChar     *fmt,
            u_scanf_spec    *spec)
{
    const UChar *s = fmt;
    const UChar *backup;
    u_scanf_spec_info *info = &(spec->fInfo);

    /* initialize spec to default values */
    spec->fArgPos             = -1;

    info->fWidth        = -1;
    info->fSpec         = 0x0000;
    info->fPadChar      = 0x0020;
    info->fSkipArg      = FALSE;
    info->fIsLongDouble = FALSE;
    info->fIsShort      = FALSE;
    info->fIsLong       = FALSE;
    info->fIsLongLong   = FALSE;
    info->fIsString     = TRUE;


    /* skip over the initial '%' */
    s++;

    /* Check for positional argument */
    if(ISDIGIT(*s)) {

        /* Save the current position */
        backup = s;

        /* handle positional parameters */
        if(ISDIGIT(*s)) {
            spec->fArgPos = (int) (*s++ - DIGIT_ZERO);

            while(ISDIGIT(*s)) {
                spec->fArgPos *= 10;
                spec->fArgPos += (int) (*s++ - DIGIT_ZERO);
            }
        }

        /* if there is no '$', don't read anything */
        if(*s != SPEC_DOLLARSIGN) {
            spec->fArgPos = -1;
            s = backup;
        }
        /* munge the '$' */
        else
            s++;
    }

    /* Get any format flags */
    while(ISFLAG(*s)) {
        switch(*s++) {

            /* skip argument */
        case FLAG_ASTERISK:
            info->fSkipArg = TRUE;
            break;

            /* pad character specified */
        case FLAG_PAREN:

            /* first four characters are hex values for pad char */
            info->fPadChar = (UChar)ufmt_digitvalue(*s++);
            info->fPadChar = (UChar)((info->fPadChar * 16) + ufmt_digitvalue(*s++));
            info->fPadChar = (UChar)((info->fPadChar * 16) + ufmt_digitvalue(*s++));
            info->fPadChar = (UChar)((info->fPadChar * 16) + ufmt_digitvalue(*s++));

            /* final character is ignored */
            s++;

            break;
        }
    }

    /* Get the width */
    if(ISDIGIT(*s)){
        info->fWidth = (int) (*s++ - DIGIT_ZERO);

        while(ISDIGIT(*s)) {
            info->fWidth *= 10;
            info->fWidth += (int) (*s++ - DIGIT_ZERO);
        }
    }

    /* Get any modifiers */
    if(ISMOD(*s)) {
        switch(*s++) {

            /* short */
        case MOD_H:
            info->fIsShort = TRUE;
            break;

            /* long or long long */
        case MOD_LOWERL:
            if(*s == MOD_LOWERL) {
                info->fIsLongLong = TRUE;
                /* skip over the next 'l' */
                s++;
            }
            else
                info->fIsLong = TRUE;
            break;

            /* long double */
        case MOD_L:
            info->fIsLongDouble = TRUE;
            break;
        }
    }

    /* finally, get the specifier letter */
    info->fSpec = *s++;

    /* return # of characters in this specifier */
    return (int32_t)(s - fmt);
}

#define UP_PERCENT 0x0025


/* ANSI style formatting */
/* Use US-ASCII characters only for formatting */

/* % */
#define UFMT_SIMPLE_PERCENT {ufmt_simple_percent, u_scanf_simple_percent_handler}
/* s */
#define UFMT_STRING         {ufmt_string, u_scanf_string_handler}
/* c */
#define UFMT_CHAR           {ufmt_string, u_scanf_char_handler}
/* d, i */
#define UFMT_INT            {ufmt_int, u_scanf_integer_handler}
/* u */
#define UFMT_UINT           {ufmt_int, u_scanf_uinteger_handler}
/* o */
#define UFMT_OCTAL          {ufmt_int, u_scanf_octal_handler}
/* x, X */
#define UFMT_HEX            {ufmt_int, u_scanf_hex_handler}
/* f */
#define UFMT_DOUBLE         {ufmt_double, u_scanf_double_handler}
/* e, E */
#define UFMT_SCIENTIFIC     {ufmt_double, u_scanf_scientific_handler}
/* g, G */
#define UFMT_SCIDBL         {ufmt_double, u_scanf_scidbl_handler}
/* n */
#define UFMT_COUNT          {ufmt_count, u_scanf_count_handler}
/* [ */
#define UFMT_SCANSET        {ufmt_string, u_scanf_scanset_handler}

/* non-ANSI extensions */
/* Use US-ASCII characters only for formatting */

/* p */
#define UFMT_POINTER        {ufmt_pointer, u_scanf_pointer_handler}
/* V */
#define UFMT_SPELLOUT       {ufmt_double, u_scanf_spellout_handler}
/* P */
#define UFMT_PERCENT        {ufmt_double, u_scanf_percent_handler}
/* C  K is old format */
#define UFMT_UCHAR          {ufmt_uchar, u_scanf_uchar_handler}
/* S  U is old format */
#define UFMT_USTRING        {ufmt_ustring, u_scanf_ustring_handler}


#define UFMT_EMPTY {ufmt_empty, NULL}

/**
 * A u_scanf handler function.
 * A u_scanf handler is responsible for handling a single u_scanf
 * format specification, for example 'd' or 's'.
 * @param stream The UFILE to which to write output.
 * @param info A pointer to a <TT>u_scanf_spec_info</TT> struct containing
 * information on the format specification.
 * @param args A pointer to the argument data
 * @param fmt A pointer to the first character in the format string
 * following the spec.
 * @param fmtConsumed On output, set to the number of characters consumed
 * in <TT>fmt</TT>. Do nothing, if the argument isn't variable width.
 * @param argConverted The number of arguments converted and assigned, or -1 if an
 * error occurred.
 * @return The number of code points consumed during reading.
 */
typedef int32_t (*u_scanf_handler) (UFILE   *stream,
                   u_scanf_spec_info  *info,
                   ufmt_args                *args,
                   const UChar              *fmt,
                   int32_t                  *fmtConsumed,
                   int32_t                  *argConverted);

typedef struct u_scanf_info {
    ufmt_type_info info;
    u_scanf_handler handler;
} u_scanf_info;

#define USCANF_NUM_FMT_HANDLERS 108
#define USCANF_SYMBOL_BUFFER_SIZE 8

/* We do not use handlers for 0-0x1f */
#define USCANF_BASE_FMT_HANDLERS 0x20


static int32_t
u_scanf_skip_leading_ws(UFILE   *input,
                        UChar   pad)
{
    UChar   c;
    int32_t count = 0;
    UBool isNotEOF;

    /* skip all leading ws in the input */
    while( (isNotEOF = ufile_getch(input, &c)) && (c == pad || u_isWhitespace(c)) )
    {
        count++;
    }

    /* put the final character back on the input */
    if(isNotEOF)
        u_fungetc(c, input);

    return count;
}

/* TODO: Is always skipping the prefix symbol as a positive sign a good idea in all locales? */
static int32_t
u_scanf_skip_leading_positive_sign(UFILE   *input,
                                   UNumberFormat *format,
                                   UErrorCode *status)
{
    UChar   c;
    int32_t count = 0;
    UBool isNotEOF;
    UChar plusSymbol[USCANF_SYMBOL_BUFFER_SIZE];
    int32_t symbolLen;
    UErrorCode localStatus = U_ZERO_ERROR;

    if (U_SUCCESS(*status)) {
        symbolLen = unum_getSymbol(format,
            UNUM_PLUS_SIGN_SYMBOL,
            plusSymbol,
            UPRV_LENGTHOF(plusSymbol),
            &localStatus);

        if (U_SUCCESS(localStatus)) {
            /* skip all leading ws in the input */
            while( (isNotEOF = ufile_getch(input, &c)) && (count < symbolLen && c == plusSymbol[count]) )
            {
                count++;
            }

            /* put the final character back on the input */
            if(isNotEOF) {
                u_fungetc(c, input);
            }
        }
    }

    return count;
}

static int32_t
u_scanf_simple_percent_handler(UFILE        *input,
                               u_scanf_spec_info *info,
                               ufmt_args    *args,
                               const UChar  *fmt,
                               int32_t      *fmtConsumed,
                               int32_t      *argConverted)
{
    /* make sure the next character in the input is a percent */
    *argConverted = 0;
    if(u_fgetc(input) != 0x0025) {
        *argConverted = -1;
    }
    return 1;
}

static int32_t
u_scanf_count_handler(UFILE         *input,
                      u_scanf_spec_info *info,
                      ufmt_args     *args,
                      const UChar   *fmt,
                      int32_t       *fmtConsumed,
                      int32_t       *argConverted)
{
    /* in the special case of count, the u_scanf_spec_info's width */
    /* will contain the # of items converted thus far */
    if (!info->fSkipArg) {
        if (info->fIsShort)
            *(int16_t*)(args[0].ptrValue) = (int16_t)(UINT16_MAX & info->fWidth);
        else if (info->fIsLongLong)
            *(int64_t*)(args[0].ptrValue) = info->fWidth;
        else
            *(int32_t*)(args[0].ptrValue) = (int32_t)(UINT32_MAX & info->fWidth);
    }
    *argConverted = 0;

    /* we converted 0 args */
    return 0;
}

static int32_t
u_scanf_double_handler(UFILE        *input,
                       u_scanf_spec_info *info,
                       ufmt_args    *args,
                       const UChar  *fmt,
                       int32_t      *fmtConsumed,
                       int32_t      *argConverted)
{
    int32_t         len;
    double          num;
    UNumberFormat   *format;
    int32_t         parsePos    = 0;
    int32_t         skipped;
    UErrorCode      status      = U_ZERO_ERROR;


    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* get the formatter */
    format = u_locbund_getNumberFormat(&input->str.fBundle, UNUM_DECIMAL);

    /* handle error */
    if(format == 0)
        return 0;

    /* Skip the positive prefix. ICU normally can't handle this due to strict parsing. */
    skipped += u_scanf_skip_leading_positive_sign(input, format, &status);

    /* parse the number */
    num = unum_parseDouble(format, input->str.fPos, len, &parsePos, &status);

    if (!info->fSkipArg) {
        if (info->fIsLong)
            *(double*)(args[0].ptrValue) = num;
        else if (info->fIsLongDouble)
            *(long double*)(args[0].ptrValue) = num;
        else
            *(float*)(args[0].ptrValue) = (float)num;
    }

    /* mask off any necessary bits */
    /*  if(! info->fIsLong_double)
    num &= DBL_MAX;*/

    /* update the input's position to reflect consumed data */
    input->str.fPos += parsePos;

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return parsePos + skipped;
}

#define UPRINTF_SYMBOL_BUFFER_SIZE 8

static int32_t
u_scanf_scientific_handler(UFILE        *input,
                           u_scanf_spec_info *info,
                           ufmt_args    *args,
                           const UChar  *fmt,
                           int32_t      *fmtConsumed,
                           int32_t      *argConverted)
{
    int32_t         len;
    double          num;
    UNumberFormat   *format;
    int32_t         parsePos    = 0;
    int32_t         skipped;
    UErrorCode      status      = U_ZERO_ERROR;
    UChar srcExpBuf[UPRINTF_SYMBOL_BUFFER_SIZE];
    int32_t srcLen, expLen;
    UChar expBuf[UPRINTF_SYMBOL_BUFFER_SIZE];


    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* get the formatter */
    format = u_locbund_getNumberFormat(&input->str.fBundle, UNUM_SCIENTIFIC);

    /* handle error */
    if(format == 0)
        return 0;

    /* set the appropriate flags on the formatter */

    srcLen = unum_getSymbol(format,
        UNUM_EXPONENTIAL_SYMBOL,
        srcExpBuf,
        sizeof(srcExpBuf),
        &status);

    /* Upper/lower case the e */
    if (info->fSpec == (UChar)0x65 /* e */) {
        expLen = u_strToLower(expBuf, (int32_t)sizeof(expBuf),
            srcExpBuf, srcLen,
            input->str.fBundle.fLocale,
            &status);
    }
    else {
        expLen = u_strToUpper(expBuf, (int32_t)sizeof(expBuf),
            srcExpBuf, srcLen,
            input->str.fBundle.fLocale,
            &status);
    }

    unum_setSymbol(format,
        UNUM_EXPONENTIAL_SYMBOL,
        expBuf,
        expLen,
        &status);




    /* Skip the positive prefix. ICU normally can't handle this due to strict parsing. */
    skipped += u_scanf_skip_leading_positive_sign(input, format, &status);

    /* parse the number */
    num = unum_parseDouble(format, input->str.fPos, len, &parsePos, &status);

    if (!info->fSkipArg) {
        if (info->fIsLong)
            *(double*)(args[0].ptrValue) = num;
        else if (info->fIsLongDouble)
            *(long double*)(args[0].ptrValue) = num;
        else
            *(float*)(args[0].ptrValue) = (float)num;
    }

    /* mask off any necessary bits */
    /*  if(! info->fIsLong_double)
    num &= DBL_MAX;*/

    /* update the input's position to reflect consumed data */
    input->str.fPos += parsePos;

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return parsePos + skipped;
}

static int32_t
u_scanf_scidbl_handler(UFILE        *input,
                       u_scanf_spec_info *info,
                       ufmt_args    *args,
                       const UChar  *fmt,
                       int32_t      *fmtConsumed,
                       int32_t      *argConverted)
{
    int32_t       len;
    double        num;
    UNumberFormat *scientificFormat, *genericFormat;
    /*int32_t       scientificResult, genericResult;*/
    double        scientificResult, genericResult;
    int32_t       scientificParsePos = 0, genericParsePos = 0, parsePos = 0;
    int32_t       skipped;
    UErrorCode    scientificStatus = U_ZERO_ERROR;
    UErrorCode    genericStatus = U_ZERO_ERROR;


    /* since we can't determine by scanning the characters whether */
    /* a number was formatted in the 'f' or 'g' styles, parse the */
    /* string with both formatters, and assume whichever one */
    /* parsed the most is the correct formatter to use */


    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* get the formatters */
    scientificFormat = u_locbund_getNumberFormat(&input->str.fBundle, UNUM_SCIENTIFIC);
    genericFormat = u_locbund_getNumberFormat(&input->str.fBundle, UNUM_DECIMAL);

    /* handle error */
    if(scientificFormat == 0 || genericFormat == 0)
        return 0;

    /* Skip the positive prefix. ICU normally can't handle this due to strict parsing. */
    skipped += u_scanf_skip_leading_positive_sign(input, genericFormat, &genericStatus);

    /* parse the number using each format*/

    scientificResult = unum_parseDouble(scientificFormat, input->str.fPos, len,
        &scientificParsePos, &scientificStatus);

    genericResult = unum_parseDouble(genericFormat, input->str.fPos, len,
        &genericParsePos, &genericStatus);

    /* determine which parse made it farther */
    if(scientificParsePos > genericParsePos) {
        /* stash the result in num */
        num = scientificResult;
        /* update the input's position to reflect consumed data */
        parsePos += scientificParsePos;
    }
    else {
        /* stash the result in num */
        num = genericResult;
        /* update the input's position to reflect consumed data */
        parsePos += genericParsePos;
    }
    input->str.fPos += parsePos;

    if (!info->fSkipArg) {
        if (info->fIsLong)
            *(double*)(args[0].ptrValue) = num;
        else if (info->fIsLongDouble)
            *(long double*)(args[0].ptrValue) = num;
        else
            *(float*)(args[0].ptrValue) = (float)num;
    }

    /* mask off any necessary bits */
    /*  if(! info->fIsLong_double)
    num &= DBL_MAX;*/

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return parsePos + skipped;
}

static int32_t
u_scanf_integer_handler(UFILE       *input,
                        u_scanf_spec_info *info,
                        ufmt_args   *args,
                        const UChar *fmt,
                        int32_t     *fmtConsumed,
                        int32_t     *argConverted)
{
    int32_t         len;
    void            *num        = (void*) (args[0].ptrValue);
    UNumberFormat   *format;
    int32_t         parsePos    = 0;
    int32_t         skipped;
    UErrorCode      status      = U_ZERO_ERROR;
    int64_t         result;


    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* get the formatter */
    format = u_locbund_getNumberFormat(&input->str.fBundle, UNUM_DECIMAL);

    /* handle error */
    if(format == 0)
        return 0;

    /* Skip the positive prefix. ICU normally can't handle this due to strict parsing. */
    skipped += u_scanf_skip_leading_positive_sign(input, format, &status);

    /* parse the number */
    result = unum_parseInt64(format, input->str.fPos, len, &parsePos, &status);

    /* mask off any necessary bits */
    if (!info->fSkipArg) {
        if (info->fIsShort)
            *(int16_t*)num = (int16_t)(UINT16_MAX & result);
        else if (info->fIsLongLong)
            *(int64_t*)num = result;
        else
            *(int32_t*)num = (int32_t)(UINT32_MAX & result);
    }

    /* update the input's position to reflect consumed data */
    input->str.fPos += parsePos;

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return parsePos + skipped;
}

static int32_t
u_scanf_uinteger_handler(UFILE          *input,
                         u_scanf_spec_info *info,
                         ufmt_args      *args,
                         const UChar    *fmt,
                         int32_t        *fmtConsumed,
                         int32_t        *argConverted)
{
    /* TODO Fix this when Numberformat handles uint64_t */
    return u_scanf_integer_handler(input, info, args, fmt, fmtConsumed, argConverted);
}

static int32_t
u_scanf_percent_handler(UFILE       *input,
                        u_scanf_spec_info *info,
                        ufmt_args   *args,
                        const UChar *fmt,
                        int32_t     *fmtConsumed,
                        int32_t     *argConverted)
{
    int32_t         len;
    double          num;
    UNumberFormat   *format;
    int32_t         parsePos    = 0;
    UErrorCode      status      = U_ZERO_ERROR;


    /* skip all ws in the input */
    u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* get the formatter */
    format = u_locbund_getNumberFormat(&input->str.fBundle, UNUM_PERCENT);

    /* handle error */
    if(format == 0)
        return 0;

    /* Skip the positive prefix. ICU normally can't handle this due to strict parsing. */
    u_scanf_skip_leading_positive_sign(input, format, &status);

    /* parse the number */
    num = unum_parseDouble(format, input->str.fPos, len, &parsePos, &status);

    if (!info->fSkipArg) {
        *(double*)(args[0].ptrValue) = num;
    }

    /* mask off any necessary bits */
    /*  if(! info->fIsLong_double)
    num &= DBL_MAX;*/

    /* update the input's position to reflect consumed data */
    input->str.fPos += parsePos;

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return parsePos;
}

static int32_t
u_scanf_string_handler(UFILE        *input,
                       u_scanf_spec_info *info,
                       ufmt_args    *args,
                       const UChar  *fmt,
                       int32_t      *fmtConsumed,
                       int32_t      *argConverted)
{
    const UChar *source;
    UConverter  *conv;
    char        *arg    = (char*)(args[0].ptrValue);
    char        *alias  = arg;
    char        *limit;
    UErrorCode  status  = U_ZERO_ERROR;
    int32_t     count;
    int32_t     skipped = 0;
    UChar       c;
    UBool       isNotEOF = FALSE;

    /* skip all ws in the input */
    if (info->fIsString) {
        skipped = u_scanf_skip_leading_ws(input, info->fPadChar);
    }

    /* get the string one character at a time, truncating to the width */
    count = 0;

    /* open the default converter */
    conv = u_getDefaultConverter(&status);

    if(U_FAILURE(status))
        return -1;

    while( (info->fWidth == -1 || count < info->fWidth)
        && (isNotEOF = ufile_getch(input, &c))
        && (!info->fIsString || (c != info->fPadChar && !u_isWhitespace(c))))
    {

        if (!info->fSkipArg) {
            /* put the character from the input onto the target */
            source = &c;
            /* Since we do this one character at a time, do it this way. */
            if (info->fWidth > 0) {
                limit = alias + info->fWidth - count;
            }
            else {
                limit = alias + ucnv_getMaxCharSize(conv);
            }

            /* convert the character to the default codepage */
            ucnv_fromUnicode(conv, &alias, limit, &source, source + 1,
                NULL, TRUE, &status);

            if(U_FAILURE(status)) {
                /* clean up */
                u_releaseDefaultConverter(conv);
                return -1;
            }
        }

        /* increment the count */
        ++count;
    }

    /* put the final character we read back on the input */
    if (!info->fSkipArg) {
        if ((info->fWidth == -1 || count < info->fWidth) && isNotEOF)
            u_fungetc(c, input);

        /* add the terminator */
        if (info->fIsString) {
            *alias = 0x00;
        }
    }

    /* clean up */
    u_releaseDefaultConverter(conv);

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return count + skipped;
}

static int32_t
u_scanf_char_handler(UFILE          *input,
                     u_scanf_spec_info *info,
                     ufmt_args      *args,
                     const UChar    *fmt,
                     int32_t        *fmtConsumed,
                     int32_t        *argConverted)
{
    if (info->fWidth < 0) {
        info->fWidth = 1;
    }
    info->fIsString = FALSE;
    return u_scanf_string_handler(input, info, args, fmt, fmtConsumed, argConverted);
}

static int32_t
u_scanf_ustring_handler(UFILE       *input,
                        u_scanf_spec_info *info,
                        ufmt_args   *args,
                        const UChar *fmt,
                        int32_t     *fmtConsumed,
                        int32_t     *argConverted)
{
    UChar   *arg     = (UChar*)(args[0].ptrValue);
    UChar   *alias     = arg;
    int32_t count;
    int32_t skipped = 0;
    UChar   c;
    UBool   isNotEOF = FALSE;

    /* skip all ws in the input */
    if (info->fIsString) {
        skipped = u_scanf_skip_leading_ws(input, info->fPadChar);
    }

    /* get the string one character at a time, truncating to the width */
    count = 0;

    while( (info->fWidth == -1 || count < info->fWidth)
        && (isNotEOF = ufile_getch(input, &c))
        && (!info->fIsString || (c != info->fPadChar && !u_isWhitespace(c))))
    {

        /* put the character from the input onto the target */
        if (!info->fSkipArg) {
            *alias++ = c;
        }

        /* increment the count */
        ++count;
    }

    /* put the final character we read back on the input */
    if (!info->fSkipArg) {
        if((info->fWidth == -1 || count < info->fWidth) && isNotEOF) {
            u_fungetc(c, input);
        }

        /* add the terminator */
        if (info->fIsString) {
            *alias = 0x0000;
        }
    }

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return count + skipped;
}

static int32_t
u_scanf_uchar_handler(UFILE         *input,
                      u_scanf_spec_info *info,
                      ufmt_args     *args,
                      const UChar   *fmt,
                      int32_t       *fmtConsumed,
                      int32_t       *argConverted)
{
    if (info->fWidth < 0) {
        info->fWidth = 1;
    }
    info->fIsString = FALSE;
    return u_scanf_ustring_handler(input, info, args, fmt, fmtConsumed, argConverted);
}

static int32_t
u_scanf_spellout_handler(UFILE          *input,
                         u_scanf_spec_info *info,
                         ufmt_args      *args,
                         const UChar    *fmt,
                         int32_t        *fmtConsumed,
                         int32_t        *argConverted)
{
    int32_t         len;
    double          num;
    UNumberFormat   *format;
    int32_t         parsePos    = 0;
    int32_t         skipped;
    UErrorCode      status      = U_ZERO_ERROR;


    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* get the formatter */
    format = u_locbund_getNumberFormat(&input->str.fBundle, UNUM_SPELLOUT);

    /* handle error */
    if(format == 0)
        return 0;

    /* Skip the positive prefix. ICU normally can't handle this due to strict parsing. */
    /* This is not applicable to RBNF. */
    /*skipped += u_scanf_skip_leading_positive_sign(input, format, &status);*/

    /* parse the number */
    num = unum_parseDouble(format, input->str.fPos, len, &parsePos, &status);

    if (!info->fSkipArg) {
        *(double*)(args[0].ptrValue) = num;
    }

    /* mask off any necessary bits */
    /*  if(! info->fIsLong_double)
    num &= DBL_MAX;*/

    /* update the input's position to reflect consumed data */
    input->str.fPos += parsePos;

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return parsePos + skipped;
}

static int32_t
u_scanf_hex_handler(UFILE       *input,
                    u_scanf_spec_info *info,
                    ufmt_args   *args,
                    const UChar *fmt,
                    int32_t     *fmtConsumed,
                    int32_t     *argConverted)
{
    int32_t     len;
    int32_t     skipped;
    void        *num    = (void*) (args[0].ptrValue);
    int64_t     result;

    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* check for alternate form */
    if( *(input->str.fPos) == 0x0030 &&
        (*(input->str.fPos + 1) == 0x0078 || *(input->str.fPos + 1) == 0x0058) ) {

        /* skip the '0' and 'x' or 'X' if present */
        input->str.fPos += 2;
        len -= 2;
    }

    /* parse the number */
    result = ufmt_uto64(input->str.fPos, &len, 16);

    /* update the input's position to reflect consumed data */
    input->str.fPos += len;

    /* mask off any necessary bits */
    if (!info->fSkipArg) {
        if (info->fIsShort)
            *(int16_t*)num = (int16_t)(UINT16_MAX & result);
        else if (info->fIsLongLong)
            *(int64_t*)num = result;
        else
            *(int32_t*)num = (int32_t)(UINT32_MAX & result);
    }

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return len + skipped;
}

static int32_t
u_scanf_octal_handler(UFILE         *input,
                      u_scanf_spec_info *info,
                      ufmt_args     *args,
                      const UChar   *fmt,
                      int32_t       *fmtConsumed,
                      int32_t       *argConverted)
{
    int32_t     len;
    int32_t     skipped;
    void        *num         = (void*) (args[0].ptrValue);
    int64_t     result;

    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1)
        len = ufmt_min(len, info->fWidth);

    /* parse the number */
    result = ufmt_uto64(input->str.fPos, &len, 8);

    /* update the input's position to reflect consumed data */
    input->str.fPos += len;

    /* mask off any necessary bits */
    if (!info->fSkipArg) {
        if (info->fIsShort)
            *(int16_t*)num = (int16_t)(UINT16_MAX & result);
        else if (info->fIsLongLong)
            *(int64_t*)num = result;
        else
            *(int32_t*)num = (int32_t)(UINT32_MAX & result);
    }

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return len + skipped;
}

static int32_t
u_scanf_pointer_handler(UFILE       *input,
                        u_scanf_spec_info *info,
                        ufmt_args   *args,
                        const UChar *fmt,
                        int32_t     *fmtConsumed,
                        int32_t     *argConverted)
{
    int32_t len;
    int32_t skipped;
    void    *result;
    void    **p     = (void**)(args[0].ptrValue);


    /* skip all ws in the input */
    skipped = u_scanf_skip_leading_ws(input, info->fPadChar);

    /* fill the input's internal buffer */
    ufile_fill_uchar_buffer(input);

    /* determine the size of the input's buffer */
    len = (int32_t)(input->str.fLimit - input->str.fPos);

    /* truncate to the width, if specified */
    if(info->fWidth != -1) {
        len = ufmt_min(len, info->fWidth);
    }

    /* Make sure that we don't consume too much */
    if (len > (int32_t)(sizeof(void*)*2)) {
        len = (int32_t)(sizeof(void*)*2);
    }

    /* parse the pointer - assign to temporary value */
    result = ufmt_utop(input->str.fPos, &len);

    if (!info->fSkipArg) {
        *p = result;
    }

    /* update the input's position to reflect consumed data */
    input->str.fPos += len;

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return len + skipped;
}

static int32_t
u_scanf_scanset_handler(UFILE       *input,
                        u_scanf_spec_info *info,
                        ufmt_args   *args,
                        const UChar *fmt,
                        int32_t     *fmtConsumed,
                        int32_t     *argConverted)
{
    USet        *scanset;
    UErrorCode  status = U_ZERO_ERROR;
    int32_t     chLeft = INT32_MAX;
    UChar32     c;
    UChar       *alias = (UChar*) (args[0].ptrValue);
    UBool       isNotEOF = FALSE;
    UBool       readCharacter = FALSE;

    /* Create an empty set */
    scanset = uset_open(0, -1);

    /* Back up one to get the [ */
    fmt--;

    /* truncate to the width, if specified and alias the target */
    if(info->fWidth >= 0) {
        chLeft = info->fWidth;
    }

    /* parse the scanset from the fmt string */
    *fmtConsumed = uset_applyPattern(scanset, fmt, -1, 0, &status);

    /* verify that the parse was successful */
    if (U_SUCCESS(status)) {
        c=0;

        /* grab characters one at a time and make sure they are in the scanset */
        while(chLeft > 0) {
            if ((isNotEOF = ufile_getch32(input, &c)) && uset_contains(scanset, c)) {
                readCharacter = TRUE;
                if (!info->fSkipArg) {
                    int32_t idx = 0;
                    UBool isError = FALSE;

                    U16_APPEND(alias, idx, chLeft, c, isError);
                    if (isError) {
                        break;
                    }
                    alias += idx;
                }
                chLeft -= (1 + U_IS_SUPPLEMENTARY(c));
            }
            else {
                /* if the character's not in the scanset, break out */
                break;
            }
        }

        /* put the final character we read back on the input */
        if(isNotEOF && chLeft > 0) {
            u_fungetc(c, input);
        }
    }

    uset_close(scanset);

    /* if we didn't match at least 1 character, fail */
    if(!readCharacter)
        return -1;
    /* otherwise, add the terminator */
    else if (!info->fSkipArg) {
        *alias = 0x00;
    }

    /* we converted 1 arg */
    *argConverted = !info->fSkipArg;
    return (info->fWidth >= 0 ? info->fWidth : INT32_MAX) - chLeft;
}

/* Use US-ASCII characters only for formatting. Most codepages have
 characters 20-7F from Unicode. Using any other codepage specific
 characters will make it very difficult to format the string on
 non-Unicode machines */
static const u_scanf_info g_u_scanf_infos[USCANF_NUM_FMT_HANDLERS] = {
/* 0x20 */
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_SIMPLE_PERCENT,UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,

/* 0x30 */
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,

/* 0x40 */
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_UCHAR,
    UFMT_EMPTY,         UFMT_SCIENTIFIC,    UFMT_EMPTY,         UFMT_SCIDBL,
#ifdef U_USE_OBSOLETE_IO_FORMATTING
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_UCHAR/*deprecated*/,
#else
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
#endif
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,

/* 0x50 */
    UFMT_PERCENT,       UFMT_EMPTY,         UFMT_EMPTY,         UFMT_USTRING,
#ifdef U_USE_OBSOLETE_IO_FORMATTING
    UFMT_EMPTY,         UFMT_USTRING/*deprecated*/,UFMT_SPELLOUT,      UFMT_EMPTY,
#else
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_SPELLOUT,      UFMT_EMPTY,
#endif
    UFMT_HEX,           UFMT_EMPTY,         UFMT_EMPTY,         UFMT_SCANSET,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,

/* 0x60 */
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_CHAR,
    UFMT_INT,           UFMT_SCIENTIFIC,    UFMT_DOUBLE,        UFMT_SCIDBL,
    UFMT_EMPTY,         UFMT_INT,           UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_COUNT,         UFMT_OCTAL,

/* 0x70 */
    UFMT_POINTER,       UFMT_EMPTY,         UFMT_EMPTY,         UFMT_STRING,
    UFMT_EMPTY,         UFMT_UINT,          UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_HEX,           UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
    UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,         UFMT_EMPTY,
};

U_CFUNC int32_t
u_scanf_parse(UFILE     *f,
            const UChar *patternSpecification,
            va_list     ap)
{
    const UChar     *alias;
    int32_t         count, converted, argConsumed, cpConsumed;
    uint16_t        handlerNum;

    ufmt_args       args;
    u_scanf_spec    spec;
    ufmt_type_info  info;
    u_scanf_handler handler;

    /* alias the pattern */
    alias = patternSpecification;

    /* haven't converted anything yet */
    argConsumed = 0;
    converted = 0;
    cpConsumed = 0;

    /* iterate through the pattern */
    for(;;) {

        /* match any characters up to the next '%' */
        while(*alias != UP_PERCENT && *alias != 0x0000 && u_fgetc(f) == *alias) {
            alias++;
        }

        /* if we aren't at a '%', or if we're at end of string, break*/
        if(*alias != UP_PERCENT || *alias == 0x0000)
            break;

        /* parse the specifier */
        count = u_scanf_parse_spec(alias, &spec);

        /* update the pointer in pattern */
        alias += count;

        handlerNum = (uint16_t)(spec.fInfo.fSpec - USCANF_BASE_FMT_HANDLERS);
        if (handlerNum < USCANF_NUM_FMT_HANDLERS) {
            /* skip the argument, if necessary */
            /* query the info function for argument information */
            info = g_u_scanf_infos[ handlerNum ].info;
            if (info != ufmt_count && u_feof(f)) {
                break;
            }
            else if(spec.fInfo.fSkipArg) {
                args.ptrValue = NULL;
            }
            else {
                switch(info) {
                case ufmt_count:
                    /* set the spec's width to the # of items converted */
                    spec.fInfo.fWidth = cpConsumed;
                    U_FALLTHROUGH;
                case ufmt_char:
                case ufmt_uchar:
                case ufmt_int:
                case ufmt_string:
                case ufmt_ustring:
                case ufmt_pointer:
                case ufmt_float:
                case ufmt_double:
                    args.ptrValue = va_arg(ap, void*);
                    break;

                default:
                    /* else args is ignored */
                    args.ptrValue = NULL;
                    break;
                }
            }

            /* call the handler function */
            handler = g_u_scanf_infos[ handlerNum ].handler;
            if(handler != 0) {

                /* reset count to 1 so that += for alias works. */
                count = 1;

                cpConsumed += (*handler)(f, &spec.fInfo, &args, alias, &count, &argConsumed);

                /* if the handler encountered an error condition, break */
                if(argConsumed < 0) {
                    converted = -1;
                    break;
                }

                /* add to the # of items converted */
                converted += argConsumed;

                /* update the pointer in pattern */
                alias += count-1;
            }
            /* else do nothing */
        }
        /* else do nothing */

        /* just ignore unknown tags */
    }

    /* return # of items converted */
    return converted;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
