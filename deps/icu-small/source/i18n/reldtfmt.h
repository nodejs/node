// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2016, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef RELDTFMT_H
#define RELDTFMT_H

#include "unicode/utypes.h"

/**
 * \file
 * \brief C++ API: Format and parse relative dates and times.
 */

#if !UCONFIG_NO_FORMATTING

#include "unicode/datefmt.h"
#include "unicode/smpdtfmt.h"
#include "unicode/brkiter.h"

U_NAMESPACE_BEGIN

// forward declarations
class DateFormatSymbols;
class SimpleFormatter;

// internal structure used for caching strings
struct URelativeString;

/**
 * This class is normally accessed using the kRelative or k...Relative values of EStyle as
 * parameters to DateFormat::createDateInstance.
 *
 * Example:
 *     DateFormat *fullrelative = DateFormat::createDateInstance(DateFormat::kFullRelative, loc);
 *
 * @internal ICU 3.8
 */

class RelativeDateFormat : public DateFormat {
public:
    RelativeDateFormat( UDateFormatStyle timeStyle, UDateFormatStyle dateStyle, const Locale& locale, UErrorCode& status);

    // overrides
    /**
     * Copy constructor.
     * @internal ICU 3.8
     */
    RelativeDateFormat(const RelativeDateFormat&);

    /**
     * Assignment operator.
     * @internal ICU 3.8
     */
    RelativeDateFormat& operator=(const RelativeDateFormat&);

    /**
     * Destructor.
     * @internal ICU 3.8
     */
    virtual ~RelativeDateFormat();

    /**
     * Clone this Format object polymorphically. The caller owns the result and
     * should delete it when done.
     * @return    A copy of the object.
     * @internal ICU 3.8
     */
    virtual Format* clone(void) const;

    /**
     * Return true if the given Format objects are semantically equal. Objects
     * of different subclasses are considered unequal.
     * @param other    the object to be compared with.
     * @return         true if the given Format objects are semantically equal.
     * @internal ICU 3.8
     */
    virtual UBool operator==(const Format& other) const;


    using DateFormat::format;

    /**
     * Format a date or time, which is the standard millis since 24:00 GMT, Jan
     * 1, 1970. Overrides DateFormat pure virtual method.
     * <P>
     * Example: using the US locale: "yyyy.MM.dd e 'at' HH:mm:ss zzz" ->>
     * 1996.07.10 AD at 15:08:56 PDT
     *
     * @param cal       Calendar set to the date and time to be formatted
     *                  into a date/time string.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       The formatting position. On input: an alignment field,
     *                  if desired. On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @internal ICU 3.8
     */
    virtual UnicodeString& format(  Calendar& cal,
                                    UnicodeString& appendTo,
                                    FieldPosition& pos) const;

    /**
     * Format an object to produce a string. This method handles Formattable
     * objects with a UDate type. If a the Formattable object type is not a Date,
     * then it returns a failing UErrorCode.
     *
     * @param obj       The object to format. Must be a Date.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @param status    Output param filled with success/failure status.
     * @return          Reference to 'appendTo' parameter.
     * @internal ICU 3.8
     */
    virtual UnicodeString& format(const Formattable& obj,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos,
                                  UErrorCode& status) const;


    /**
     * Parse a date/time string beginning at the given parse position. For
     * example, a time text "07/10/96 4:5 PM, PDT" will be parsed into a Date
     * that is equivalent to Date(837039928046).
     * <P>
     * By default, parsing is lenient: If the input is not in the form used by
     * this object's format method but can still be parsed as a date, then the
     * parse succeeds. Clients may insist on strict adherence to the format by
     * calling setLenient(false).
     *
     * @param text  The date/time string to be parsed
     * @param cal   a Calendar set to the date and time to be formatted
     *              into a date/time string.
     * @param pos   On input, the position at which to start parsing; on
     *              output, the position at which parsing terminated, or the
     *              start position if the parse failed.
     * @return      A valid UDate if the input could be parsed.
     * @internal ICU 3.8
     */
    virtual void parse( const UnicodeString& text,
                        Calendar& cal,
                        ParsePosition& pos) const;

    /**
     * Parse a date/time string starting at the given parse position. For
     * example, a time text "07/10/96 4:5 PM, PDT" will be parsed into a Date
     * that is equivalent to Date(837039928046).
     * <P>
     * By default, parsing is lenient: If the input is not in the form used by
     * this object's format method but can still be parsed as a date, then the
     * parse succeeds. Clients may insist on strict adherence to the format by
     * calling setLenient(false).
     *
     * @see DateFormat::setLenient(boolean)
     *
     * @param text  The date/time string to be parsed
     * @param pos   On input, the position at which to start parsing; on
     *              output, the position at which parsing terminated, or the
     *              start position if the parse failed.
     * @return      A valid UDate if the input could be parsed.
     * @internal ICU 3.8
     */
    UDate parse( const UnicodeString& text,
                 ParsePosition& pos) const;


    /**
     * Parse a date/time string. For example, a time text "07/10/96 4:5 PM, PDT"
     * will be parsed into a UDate that is equivalent to Date(837039928046).
     * Parsing begins at the beginning of the string and proceeds as far as
     * possible.  Assuming no parse errors were encountered, this function
     * doesn't return any information about how much of the string was consumed
     * by the parsing.  If you need that information, use the version of
     * parse() that takes a ParsePosition.
     *
     * @param text  The date/time string to be parsed
     * @param status Filled in with U_ZERO_ERROR if the parse was successful, and with
     *              an error value if there was a parse error.
     * @return      A valid UDate if the input could be parsed.
     * @internal ICU 3.8
     */
    virtual UDate parse( const UnicodeString& text,
                        UErrorCode& status) const;

    /**
     * Return a single pattern string generated by combining the patterns for the
     * date and time formatters associated with this object.
     * @param result Output param to receive the pattern.
     * @return       A reference to 'result'.
     * @internal ICU 4.2 technology preview
     */
    virtual UnicodeString& toPattern(UnicodeString& result, UErrorCode& status) const;

    /**
     * Get the date pattern for the the date formatter associated with this object.
     * @param result Output param to receive the date pattern.
     * @return       A reference to 'result'.
     * @internal ICU 4.2 technology preview
     */
    virtual UnicodeString& toPatternDate(UnicodeString& result, UErrorCode& status) const;

    /**
     * Get the time pattern for the the time formatter associated with this object.
     * @param result Output param to receive the time pattern.
     * @return       A reference to 'result'.
     * @internal ICU 4.2 technology preview
     */
    virtual UnicodeString& toPatternTime(UnicodeString& result, UErrorCode& status) const;

    /**
     * Apply the given unlocalized date & time pattern strings to this relative date format.
     * (i.e., after this call, this formatter will format dates and times according to
     * the new patterns)
     *
     * @param datePattern   The date pattern to be applied.
     * @param timePattern   The time pattern to be applied.
     * @internal ICU 4.2 technology preview
     */
    virtual void applyPatterns(const UnicodeString& datePattern, const UnicodeString& timePattern, UErrorCode &status);

    /**
     * Gets the date/time formatting symbols (this is an object carrying
     * the various strings and other symbols used in formatting: e.g., month
     * names and abbreviations, time zone names, AM/PM strings, etc.)
     * @return a copy of the date-time formatting data associated
     * with this date-time formatter.
     * @internal ICU 4.8
     */
    virtual const DateFormatSymbols* getDateFormatSymbols(void) const;

    /* Cannot use #ifndef U_HIDE_DRAFT_API for the following draft method since it is virtual */
    /**
     * Set a particular UDisplayContext value in the formatter, such as
     * UDISPCTX_CAPITALIZATION_FOR_STANDALONE. Note: For getContext, see
     * DateFormat.
     * @param value The UDisplayContext value to set.
     * @param status Input/output status. If at entry this indicates a failure
     *               status, the function will do nothing; otherwise this will be
     *               updated with any new status from the function.
     * @internal ICU 53
     */
    virtual void setContext(UDisplayContext value, UErrorCode& status);

private:
    SimpleDateFormat *fDateTimeFormatter;
    UnicodeString fDatePattern;
    UnicodeString fTimePattern;
    SimpleFormatter *fCombinedFormat;  // the {0} {1} format.

    UDateFormatStyle fDateStyle;
    Locale  fLocale;

    int32_t fDatesLen;    // Length of array
    URelativeString *fDates; // array of strings

    UBool fCombinedHasDateAtStart;
    UBool fCapitalizationInfoSet;
    UBool fCapitalizationOfRelativeUnitsForUIListMenu;
    UBool fCapitalizationOfRelativeUnitsForStandAlone;
#if !UCONFIG_NO_BREAK_ITERATION
    BreakIterator* fCapitalizationBrkIter;
#else
    UObject* fCapitalizationBrkIter;
#endif

    /**
     * Get the string at a specific offset.
     * @param day day offset ( -1, 0, 1, etc.. )
     * @param len on output, length of string.
     * @return the string, or NULL if none at that location.
     */
    const UChar *getStringForDay(int32_t day, int32_t &len, UErrorCode &status) const;

    /**
     * Load the Date string array
     */
    void loadDates(UErrorCode &status);

    /**
     * Set fCapitalizationOfRelativeUnitsForUIListMenu, fCapitalizationOfRelativeUnitsForStandAlone
     */
    void initCapitalizationContextInfo(const Locale& thelocale);

    /**
     * @return the number of days in "until-now"
     */
    static int32_t dayDifference(Calendar &until, UErrorCode &status);

    /**
     * initializes fCalendar from parameters.  Returns fCalendar as a convenience.
     * @param adoptZone  Zone to be adopted, or NULL for TimeZone::createDefault().
     * @param locale Locale of the calendar
     * @param status Error code
     * @return the newly constructed fCalendar
     * @internal ICU 3.8
     */
    Calendar* initializeCalendar(TimeZone* adoptZone, const Locale& locale, UErrorCode& status);

public:
    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       erived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @internal ICU 3.8
     */
    U_I18N_API static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @internal ICU 3.8
     */
    virtual UClassID getDynamicClassID(void) const;
};


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // RELDTFMT_H
