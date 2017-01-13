// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1998-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File read.h
*
* Modification History:
*
*   Date        Name        Description
*   05/26/99    stephen     Creation.
*   5/10/01     Ram         removed ustdio dependency
*******************************************************************************
*/

#ifndef READ_H
#define READ_H 1

#include "unicode/utypes.h"
#include "ustr.h"
#include "ucbuf.h"

/* The types of tokens which may be returned by getNextToken.
   NOTE: Keep these in sync with tokenNames in parse.c */
enum ETokenType
{
    TOK_STRING,          /* A string token, such as "MonthNames" */
    TOK_OPEN_BRACE,      /* An opening brace character */
    TOK_CLOSE_BRACE,     /* A closing brace character */
    TOK_COMMA,           /* A comma */
    TOK_COLON,           /* A colon */

    TOK_EOF,             /* End of the file has been reached successfully */
    TOK_ERROR,           /* An error, such an unterminated quoted string */
    TOK_TOKEN_COUNT      /* Number of "real" token types */
};

U_CFUNC UChar32 unescape(UCHARBUF *buf, UErrorCode *status);

U_CFUNC void resetLineNumber(void);

U_CFUNC enum ETokenType
getNextToken(UCHARBUF *buf,
             struct UString *token,
             uint32_t *linenumber, /* out: linenumber of token */
             struct UString *comment,
             UErrorCode *status);

#endif
