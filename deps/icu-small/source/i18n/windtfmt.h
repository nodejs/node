// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2005-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINDTFMT.H
*
********************************************************************************
*/

#ifndef __WINDTFMT
#define __WINDTFMT

#include "unicode/utypes.h"

#if U_PLATFORM_USES_ONLY_WIN32_API

#if !UCONFIG_NO_FORMATTING

#include "unicode/format.h"
#include "unicode/datefmt.h"
#include "unicode/calendar.h"
#include "unicode/ustring.h"
#include "unicode/locid.h"

/**
 * \file
 * \brief C++ API: Format dates using Windows API.
 */

U_CDECL_BEGIN
// Forward declarations for Windows types...
typedef struct _SYSTEMTIME SYSTEMTIME;
typedef struct _TIME_ZONE_INFORMATION TIME_ZONE_INFORMATION;
U_CDECL_END

U_NAMESPACE_BEGIN

class Win32DateFormat : public DateFormat
{
public:
    Win32DateFormat(DateFormat::EStyle timeStyle, DateFormat::EStyle dateStyle, const Locale &locale, UErrorCode &status);

    Win32DateFormat(const Win32DateFormat &other);

    virtual ~Win32DateFormat();

    virtual Win32DateFormat *clone() const;

    Win32DateFormat &operator=(const Win32DateFormat &other);

    UnicodeString &format(Calendar &cal, UnicodeString &appendTo, FieldPosition &pos) const;

    using DateFormat::format;

    void parse(const UnicodeString& text, Calendar& cal, ParsePosition& pos) const;

    /**
     * Set the calendar to be used by this date format. Initially, the default
     * calendar for the specified or default locale is used.  The caller should
     * not delete the Calendar object after it is adopted by this call.
     *
     * @param calendarToAdopt    Calendar object to be adopted.
     */
    virtual void adoptCalendar(Calendar* calendarToAdopt);

    /**
     * Set the calendar to be used by this date format. Initially, the default
     * calendar for the specified or default locale is used.
     *
     * @param newCalendar Calendar object to be set.
     */
    virtual void setCalendar(const Calendar& newCalendar);

    /**
     * Sets the time zone for the calendar of this DateFormat object. The caller
     * no longer owns the TimeZone object and should not delete it after this call.
     *
     * @param zoneToAdopt the TimeZone to be adopted.
     */
    virtual void adoptTimeZone(TimeZone* zoneToAdopt);

    /**
     * Sets the time zone for the calendar of this DateFormat object.
     * @param zone the new time zone.
     */
    virtual void setTimeZone(const TimeZone& zone);

    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
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
     */
    virtual UClassID getDynamicClassID(void) const;

private:
    void formatDate(const SYSTEMTIME *st, UnicodeString &appendTo) const;
    void formatTime(const SYSTEMTIME *st, UnicodeString &appendTo) const;

    UnicodeString setTimeZoneInfo(TIME_ZONE_INFORMATION *tzi, const TimeZone &zone) const;
    UnicodeString* getTimeDateFormat(const Calendar *cal, const Locale *locale, UErrorCode &status) const;

    UnicodeString *fDateTimeMsg;
    DateFormat::EStyle fTimeStyle;
    DateFormat::EStyle fDateStyle;
    Locale fLocale;
    UnicodeString fZoneID;
    TIME_ZONE_INFORMATION *fTZI;

    UnicodeString* fWindowsLocaleName; // Stores the equivalent Windows locale name.
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // U_PLATFORM_USES_ONLY_WIN32_API

#endif // __WINDTFMT
