// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **************************************************************************
 *   Copyright (c) 2002-2010, International Business Machines Corporation *
 *   and others.  All Rights Reserved.                                    *
 **************************************************************************
 *   Date        Name        Description                                  *
 *   01/28/2002  aliu        Creation.                                    *
 **************************************************************************
 */
#ifndef TRIDPARS_H
#define TRIDPARS_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/uobject.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

class Transliterator;
class UnicodeSet;
class UVector;

/**
 * Parsing component for transliterator IDs.  This class contains only
 * static members; it cannot be instantiated.  Methods in this class
 * parse various ID formats, including the following:
 *
 * A basic ID, which contains source, target, and variant, but no
 * filter and no explicit inverse.  Examples include
 * "Latin-Greek/UNGEGN" and "Null".
 *
 * A single ID, which is a basic ID plus optional filter and optional
 * explicit inverse.  Examples include "[a-zA-Z] Latin-Greek" and
 * "Lower (Upper)".
 *
 * A compound ID, which is a sequence of one or more single IDs,
 * separated by semicolons, with optional forward and reverse global
 * filters.  The global filters are UnicodeSet patterns prepended or
 * appended to the IDs, separated by semicolons.  An appended filter
 * must be enclosed in parentheses and applies in the reverse
 * direction.
 *
 * @author Alan Liu
 */
class TransliteratorIDParser /* not : public UObject because all methods are static */ {

 public:

    /**
     * A structure containing the parsed data of a filtered ID, that
     * is, a basic ID optionally with a filter.
     *
     * 'source' and 'target' will always be non-null.  The 'variant'
     * will be non-null only if a non-empty variant was parsed.
     *
     * 'sawSource' is true if there was an explicit source in the
     * parsed id.  If there was no explicit source, then an implied
     * source of ANY is returned and 'sawSource' is set to false.
     * 
     * 'filter' is the parsed filter pattern, or null if there was no
     * filter.
     */
    class Specs : public UMemory {
    public:
        UnicodeString source; // not null
        UnicodeString target; // not null
        UnicodeString variant; // may be null
        UnicodeString filter; // may be null
        UBool sawSource;
        Specs(const UnicodeString& s, const UnicodeString& t,
              const UnicodeString& v, UBool sawS,
              const UnicodeString& f);

    private:

        Specs(const Specs &other); // forbid copying of this class
        Specs &operator=(const Specs &other); // forbid copying of this class
    };

    /**
     * A structure containing the canonicalized data of a filtered ID,
     * that is, a basic ID optionally with a filter.
     *
     * 'canonID' is always non-null.  It may be the empty string "".
     * It is the id that should be assigned to the created
     * transliterator.  It _cannot_ be instantiated directly.
     *
     * 'basicID' is always non-null and non-empty.  It is always of
     * the form S-T or S-T/V.  It is designed to be fed to low-level
     * instantiation code that only understands these two formats.
     *
     * 'filter' may be null, if there is none, or non-null and
     * non-empty.
     */
    class SingleID : public UMemory {
    public:
        UnicodeString canonID;
        UnicodeString basicID;
        UnicodeString filter;
        SingleID(const UnicodeString& c, const UnicodeString& b,
                 const UnicodeString& f);
        SingleID(const UnicodeString& c, const UnicodeString& b);
        Transliterator* createInstance();

    private:

        SingleID(const SingleID &other); // forbid copying of this class
        SingleID &operator=(const SingleID &other); // forbid copying of this class
    };

    /**
     * Parse a filter ID, that is, an ID of the general form
     * "[f1] s1-t1/v1", with the filters optional, and the variants optional.
     * @param id the id to be parsed
     * @param pos INPUT-OUTPUT parameter.  On input, the position of
     * the first character to parse.  On output, the position after
     * the last character parsed.
     * @return a SingleID object or null if the parse fails
     */
    static SingleID* parseFilterID(const UnicodeString& id, int32_t& pos);

    /**
     * Parse a single ID, that is, an ID of the general form
     * "[f1] s1-t1/v1 ([f2] s2-t3/v2)", with the parenthesized element
     * optional, the filters optional, and the variants optional.
     * @param id the id to be parsed
     * @param pos INPUT-OUTPUT parameter.  On input, the position of
     * the first character to parse.  On output, the position after
     * the last character parsed.
     * @param dir the direction.  If the direction is REVERSE then the
     * SingleID is constructed for the reverse direction.
     * @return a SingleID object or null
     */
    static SingleID* parseSingleID(const UnicodeString& id, int32_t& pos,
                                  int32_t dir, UErrorCode& status);

    /**
     * Parse a global filter of the form "[f]" or "([f])", depending
     * on 'withParens'.
     * @param id the pattern the parse
     * @param pos INPUT-OUTPUT parameter.  On input, the position of
     * the first character to parse.  On output, the position after
     * the last character parsed.
     * @param dir the direction.
     * @param withParens INPUT-OUTPUT parameter.  On entry, if
     * withParens[0] is 0, then parens are disallowed.  If it is 1,
     * then parens are required.  If it is -1, then parens are
     * optional, and the return result will be set to 0 or 1.
     * @param canonID OUTPUT parameter.  The pattern for the filter
     * added to the canonID, either at the end, if dir is FORWARD, or
     * at the start, if dir is REVERSE.  The pattern will be enclosed
     * in parentheses if appropriate, and will be suffixed with an
     * ID_DELIM character.  May be null.
     * @return a UnicodeSet object or null.  A non-null results
     * indicates a successful parse, regardless of whether the filter
     * applies to the given direction.  The caller should discard it
     * if withParens != (dir == REVERSE).
     */
    static UnicodeSet* parseGlobalFilter(const UnicodeString& id, int32_t& pos,
                                         int32_t dir,
                                         int32_t& withParens,
                                         UnicodeString* canonID);

    /**
     * Parse a compound ID, consisting of an optional forward global
     * filter, a separator, one or more single IDs delimited by
     * separators, an an optional reverse global filter.  The
     * separator is a semicolon.  The global filters are UnicodeSet
     * patterns.  The reverse global filter must be enclosed in
     * parentheses.
     * @param id the pattern the parse
     * @param dir the direction.
     * @param canonID OUTPUT parameter that receives the canonical ID,
     * consisting of canonical IDs for all elements, as returned by
     * parseSingleID(), separated by semicolons.  Previous contents
     * are discarded.
     * @param list OUTPUT parameter that receives a list of SingleID
     * objects representing the parsed IDs.  Previous contents are
     * discarded.
     * @param globalFilter OUTPUT parameter that receives a pointer to
     * a newly created global filter for this ID in this direction, or
     * null if there is none.
     * @return true if the parse succeeds, that is, if the entire
     * id is consumed without syntax error.
     */
    static UBool parseCompoundID(const UnicodeString& id, int32_t dir,
                                 UnicodeString& canonID,
                                 UVector& list,
                                 UnicodeSet*& globalFilter);

    /**
     * Convert the elements of the 'list' vector, which are SingleID
     * objects, into actual Transliterator objects.  In the course of
     * this, some (or all) entries may be removed.  If all entries
     * are removed, the Null transliterator will be added.
     *
     * Delete entries with empty basicIDs; these are generated by
     * elements like "(A)" in the forward direction, or "A()" in
     * the reverse.  THIS MAY RESULT IN AN EMPTY VECTOR.  Convert
     * SingleID entries to actual transliterators.
     *
     * @param list vector of SingleID objects.  On exit, vector
     * of one or more Transliterators.
     * @param ec Output param to receive a success or an error code.
     * @return new value of insertIndex.  The index will shift if
     * there are empty items, like "(Lower)", with indices less than
     * insertIndex.
     */
    static void instantiateList(UVector& list,
                                UErrorCode& ec);

    /**
     * Parse an ID into pieces.  Take IDs of the form T, T/V, S-T,
     * S-T/V, or S/V-T.  If the source is missing, return a source of
     * ANY.
     * @param id the id string, in any of several forms
     * @param source          the given source.
     * @param target          the given target.
     * @param variant         the given variant
     * @param isSourcePresent If TRUE then the source is present. 
     *                        If the source is not present, ANY will be
     *                        given as the source, and isSourcePresent will be null
     * @return an array of 4 strings: source, target, variant, and
     * isSourcePresent.  If the source is not present, ANY will be
     * given as the source, and isSourcePresent will be null.  Otherwise
     * isSourcePresent will be non-null.  The target may be empty if the
     * id is not well-formed.  The variant may be empty.
     */
    static void IDtoSTV(const UnicodeString& id,
                        UnicodeString& source,
                        UnicodeString& target,
                        UnicodeString& variant,
                        UBool& isSourcePresent);

    /**
     * Given source, target, and variant strings, concatenate them into a
     * full ID.  If the source is empty, then "Any" will be used for the
     * source, so the ID will always be of the form s-t/v or s-t.
     */
    static void STVtoID(const UnicodeString& source,
                        const UnicodeString& target,
                        const UnicodeString& variant,
                        UnicodeString& id);

    /**
     * Register two targets as being inverses of one another.  For
     * example, calling registerSpecialInverse("NFC", "NFD", true) causes
     * Transliterator to form the following inverse relationships:
     *
     * <pre>NFC => NFD
     * Any-NFC => Any-NFD
     * NFD => NFC
     * Any-NFD => Any-NFC</pre>
     *
     * (Without the special inverse registration, the inverse of NFC
     * would be NFC-Any.)  Note that NFD is shorthand for Any-NFD, but
     * that the presence or absence of "Any-" is preserved.
     *
     * <p>The relationship is symmetrical; registering (a, b) is
     * equivalent to registering (b, a).
     *
     * <p>The relevant IDs must still be registered separately as
     * factories or classes.
     *
     * <p>Only the targets are specified.  Special inverses always
     * have the form Any-Target1 <=> Any-Target2.  The target should
     * have canonical casing (the casing desired to be produced when
     * an inverse is formed) and should contain no whitespace or other
     * extraneous characters.
     *
     * @param target the target against which to register the inverse
     * @param inverseTarget the inverse of target, that is
     * Any-target.getInverse() => Any-inverseTarget
     * @param bidirectional if true, register the reverse relation
     * as well, that is, Any-inverseTarget.getInverse() => Any-target
     */
    static void registerSpecialInverse(const UnicodeString& target,
                                       const UnicodeString& inverseTarget,
                                       UBool bidirectional,
                                       UErrorCode &status);

    /**
     * Free static memory.
     */
    static void cleanup();

 private:
    //----------------------------------------------------------------
    // Private implementation
    //----------------------------------------------------------------

    // forbid instantiation
    TransliteratorIDParser();

    /**
     * Parse an ID into component pieces.  Take IDs of the form T,
     * T/V, S-T, S-T/V, or S/V-T.  If the source is missing, return a
     * source of ANY.
     * @param id the id string, in any of several forms
     * @param pos INPUT-OUTPUT parameter.  On input, pos[0] is the
     * offset of the first character to parse in id.  On output,
     * pos[0] is the offset after the last parsed character.  If the
     * parse failed, pos[0] will be unchanged.
     * @param allowFilter if true, a UnicodeSet pattern is allowed
     * at any location between specs or delimiters, and is returned
     * as the fifth string in the array.
     * @return a Specs object, or null if the parse failed.  If
     * neither source nor target was seen in the parsed id, then the
     * parse fails.  If allowFilter is true, then the parsed filter
     * pattern is returned in the Specs object, otherwise the returned
     * filter reference is null.  If the parse fails for any reason
     * null is returned.
     */
    static Specs* parseFilterID(const UnicodeString& id, int32_t& pos,
                                UBool allowFilter);

    /**
     * Givens a Specs object, convert it to a SingleID object.  The
     * Spec object is a more unprocessed parse result.  The SingleID
     * object contains information about canonical and basic IDs.
     * @param specs the given Specs object.
     * @param dir   either FORWARD or REVERSE.
     * @return a SingleID; never returns null.  Returned object always
     * has 'filter' field of null.
     */
    static SingleID* specsToID(const Specs* specs, int32_t dir);

    /**
     * Given a Specs object, return a SingleID representing the
     * special inverse of that ID.  If there is no special inverse
     * then return null.
     * @param specs the given Specs.
     * @return a SingleID or null.  Returned object always has
     * 'filter' field of null.
     */
    static SingleID* specsToSpecialInverse(const Specs& specs, UErrorCode &status);

    /**
     * Glue method to get around access problems in C++.
     * @param id the id string for the transliterator, in any of several forms
     * @param canonID the given canonical ID
     */
    static Transliterator* createBasicInstance(const UnicodeString& id,
                                               const UnicodeString* canonID);

    /**
     * Initialize static memory.
     */
    static void U_CALLCONV init(UErrorCode &status);

    friend class SingleID;
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif
