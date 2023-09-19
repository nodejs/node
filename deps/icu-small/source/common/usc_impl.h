// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1999-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File USC_IMPL.H
*
* Modification History:
*
*   Date        Name        Description
*   07/08/2002  Eric Mader  Creation.
******************************************************************************
*/

#ifndef USC_IMPL_H
#define USC_IMPL_H
#include "unicode/utypes.h"
#include "unicode/uscript.h"

/**
 * <code>UScriptRun</code> is used to find runs of characters in
 * the same script. It implements a simple iterator over an array
 * of characters. The iterator will resolve script-neutral characters
 * like punctuation into the script of the surrounding characters.
 *
 * The iterator will try to match paired punctuation. If it sees an
 * opening punctuation character, it will remember the script that
 * was assigned to that character, and assign the same script to the
 * matching closing punctuation.
 *
 * Scripts are chosen based on the <code>UScriptCode</code> enumeration.
 * No attempt is made to combine related scripts into a single run. In
 * particular, Hiragana, Katakana, and Han characters will appear in separate
 * runs.

 * Here is an example of how to iterate over script runs:
 * <pre>
 * \code
 * void printScriptRuns(const UChar *text, int32_t length)
 * {
 *     UErrorCode error = U_ZERO_ERROR;
 *     UScriptRun *scriptRun = uscript_openRun(text, testLength, &error);
 *     int32_t start = 0, limit = 0;
 *     UScriptCode code = USCRIPT_INVALID_CODE;
 *
 *     while (uscript_nextRun(&start, &limit, &code)) {
 *         printf("Script '%s' from %d to %d.\n", uscript_getName(code), start, limit);
 *     }
 *
 *     uscript_closeRun(scriptRun);
 *  }
 * </pre>
 */
struct UScriptRun;

typedef struct UScriptRun UScriptRun;

/**
 * Create a <code>UScriptRun</code> object for iterating over the given text. This object must
 * be freed using <code>uscript_closeRun()</code>. Note that this object does not copy the source text,
 * only the pointer to it. You must make sure that the pointer remains valid until you call
 * <code>uscript_closeRun()</code> or <code>uscript_setRunText()</code>.
 *
 * @param src is the address of the array of characters over which to iterate.
 *        if <code>src == NULL</code> and <code>length == 0</code>,
 *        an empty <code>UScriptRun</code> object will be returned.
 *
 * @param length is the number of characters over which to iterate.
 *
 * @param pErrorCode is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 *
 * @return the address of <code>UScriptRun</code> object which will iterate over the text,
 *         or <code>NULL</code> if the operation failed.
 */
U_CAPI UScriptRun * U_EXPORT2
uscript_openRun(const UChar *src, int32_t length, UErrorCode *pErrorCode);

/**
 * Frees the given <code>UScriptRun</code> object and any storage associated with it.
 * On return, scriptRun no longer points to a valid <code>UScriptRun</code> object.
 *
 * @param scriptRun is the <code>UScriptRun</code> object which will be freed.
 */
U_CAPI void U_EXPORT2
uscript_closeRun(UScriptRun *scriptRun);

/**
 * Reset the <code>UScriptRun</code> object so that it will start iterating from
 * the beginning.
 *
 * @param scriptRun is the address of the <code>UScriptRun</code> object to be reset.
 */
U_CAPI void U_EXPORT2
uscript_resetRun(UScriptRun *scriptRun);

/**
 * Change the text over which the given <code>UScriptRun</code> object iterates.
 *
 * @param scriptRun is the <code>UScriptRun</code> object which will be changed.
 *
 * @param src is the address of the new array of characters over which to iterate.
 *        If <code>src == NULL</code> and <code>length == 0</code>,
 *        the <code>UScriptRun</code> object will become empty.
 *
 * @param length is the new number of characters over which to iterate
 *
 * @param pErrorCode is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 */
U_CAPI void U_EXPORT2
uscript_setRunText(UScriptRun *scriptRun, const UChar *src, int32_t length, UErrorCode *pErrorCode);

/**
 * Advance the <code>UScriptRun</code> object to the next script run, return the start and limit
 * offsets, and the script of the run.
 *
 * @param scriptRun is the address of the <code>UScriptRun</code> object.
 *
 * @param pRunStart is a pointer to the variable to receive the starting offset of the next run.
 *        This pointer can be <code>NULL</code> if the value is not needed.
 *
 * @param pRunLimit is a pointer to the variable to receive the limit offset of the next run.
 *        This pointer can be <code>NULL</code> if the value is not needed.
 *
 * @param pRunScript is a pointer to the variable to receive the UScriptCode for the
 *        script of the current run. This pointer can be <code>NULL</code> if the value is not needed.
 *
 * @return true if there was another script run.
 */
U_CAPI UBool U_EXPORT2
uscript_nextRun(UScriptRun *scriptRun, int32_t *pRunStart, int32_t *pRunLimit, UScriptCode *pRunScript);

#endif
