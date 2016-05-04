/*
*******************************************************************************
* Copyright (C) 2009-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

/**
* \file
* \brief C API: RFC2445 VTIMEZONE support
*
* <p>This is a C wrapper around the C++ VTimeZone class.</p>
*/

#ifndef __VZONE_H
#define __VZONE_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"
#include "ztrans.h"

#ifndef UCNV_H
struct VZone;
/**
 * A UnicodeSet.  Use the vzone_* API to manipulate.  Create with
 * vzone_open*, and destroy with vzone_close.
 */
typedef struct VZone VZone;
#endif

/*********************************************************************
 * VZone API
 *********************************************************************/

/**
 * Creates a vzone from the given time zone ID.
 * @param ID The time zone ID, such as America/New_York
 * @param idLength, length of the ID parameter
 * @return A vzone object initialized by the time zone ID,
 * or NULL when the ID is unknown.
 */
U_CAPI VZone* U_EXPORT2
vzone_openID(const UChar* ID, int32_t idLength);

/**
 * Create a vzone instance by RFC2445 VTIMEZONE data
 * @param vtzdata The string including VTIMEZONE data block
 * @param vtzdataLength, length of the vtzdata
 * @param status Output param to filled in with a success or an error.
 * @return A vzone initialized by the VTIMEZONE data or
 * NULL if failed to load the rule from the VTIMEZONE data.
 */
U_CAPI VZone* U_EXPORT2
vzone_openData(const UChar* vtzdata, int32_t vtzdataLength, UErrorCode& status);

/**
 * Disposes of the storage used by a VZone object.  This function should
 * be called exactly once for objects returned by vzone_open*.
 * @param set the object to dispose of
 */
U_CAPI void U_EXPORT2
vzone_close(VZone* zone);

/**
 * Returns a copy of this object.
 * @param zone the original vzone
 * @return the newly allocated copy of the vzone
 */
U_CAPI VZone* U_EXPORT2
vzone_clone(const VZone *zone);

/**
 * Returns true if zone1 is identical to zone2
 * and vis versa.
 * @param zone1 to be checked for containment
 * @param zone2 to be checked for containment
 * @return true if the test condition is met
 */
U_CAPI UBool U_EXPORT2
vzone_equals(const VZone* zone1, const VZone* zone2);

/**
 * Gets the RFC2445 TZURL property value.  When a vzone instance was
 * created from VTIMEZONE data, the initial value is set by the TZURL 
 * property value in the data.  Otherwise, the initial value is not set.
 * @param zone, the vzone to use
 * @param url Receives the RFC2445 TZURL property value.
 * @param urlLength, length of the url
 * @return TRUE if TZURL attribute is available and value is set.
 */
U_CAPI UBool U_EXPORT2
vzone_getTZURL(VZone* zone, UChar* & url, int32_t & urlLength);

/**
 * Sets the RFC2445 TZURL property value.
 * @param zone, the vzone to use
 * @param url The TZURL property value.
 * @param urlLength, length of the url
 */
U_CAPI void U_EXPORT2
vzone_setTZURL(VZone* zone, UChar* url, int32_t urlLength);

/**
 * Gets the RFC2445 LAST-MODIFIED property value.  When a vzone instance
 * was created from VTIMEZONE data, the initial value is set by the 
 * LAST-MODIFIED property value in the data.  Otherwise, the initial value 
 * is not set.
 * @param zone, the vzone to use
 * @param lastModified Receives the last modified date.
 * @return TRUE if lastModified attribute is available and value is set.
 */
U_CAPI UBool U_EXPORT2
vzone_getLastModified(VZone* zone, UDate& lastModified);

/**
 * Sets the RFC2445 LAST-MODIFIED property value.
 * @param zone, the vzone to use
 * @param lastModified The LAST-MODIFIED date.
 */
U_CAPI void U_EXPORT2
vzone_setLastModified(VZone* zone, UDate lastModified);

/**
 * Writes RFC2445 VTIMEZONE data for this time zone
 * @param zone, the vzone to use
 * @param result Output param to filled in with the VTIMEZONE data.
 * @param resultLength, length of the result output
 * @param status Output param to filled in with a success or an error.
 */
U_CAPI void U_EXPORT2
vzone_write(VZone* zone, UChar* & result, int32_t & resultLength, UErrorCode& status);

/**
 * Writes RFC2445 VTIMEZONE data for this time zone applicalbe
 * for dates after the specified start time.
 * @param zone, the vzone to use
 * @param start The start date.
 * @param result Output param to filled in with the VTIMEZONE data.
 * @param resultLength, length of the result output
 * @param status Output param to filled in with a success or an error.
 */
U_CAPI void U_EXPORT2
vzone_writeFromStart(VZone* zone, UDate start, UChar* & result, int32_t & resultLength, UErrorCode& status);

/**
 * Writes RFC2445 VTIMEZONE data applicalbe for the specified date.
 * Some common iCalendar implementations can only handle a single time
 * zone property or a pair of standard and daylight time properties using
 * BYDAY rule with day of week (such as BYDAY=1SUN).  This method produce
 * the VTIMEZONE data which can be handled these implementations.  The rules
 * produced by this method can be used only for calculating time zone offset
 * around the specified date.
 * @param zone, the vzone to use
 * @param time The date used for rule extraction.
 * @param result Output param to filled in with the VTIMEZONE data.
 * @param status Output param to filled in with a success or an error.
 */
U_CAPI void U_EXPORT2
vzone_writeSimple(VZone* zone, UDate time, UChar* & result, int32_t & resultLength, UErrorCode& status);

/**
 * Returns the TimeZone's adjusted GMT offset (i.e., the number of milliseconds to add
 * to GMT to get local time in this time zone, taking daylight savings time into
 * account) as of a particular reference date.  The reference date is used to determine
 * whether daylight savings time is in effect and needs to be figured into the offset
 * that is returned (in other words, what is the adjusted GMT offset in this time zone
 * at this particular date and time?).  For the time zones produced by createTimeZone(),
 * the reference data is specified according to the Gregorian calendar, and the date
 * and time fields are local standard time.
 *
 * <p>Note: Don't call this method. Instead, call the getOffset(UDate...) overload,
 * which returns both the raw and the DST offset for a given time. This method
 * is retained only for backward compatibility.
 *
 * @param zone, the vzone to use
 * @param era        The reference date's era
 * @param year       The reference date's year
 * @param month      The reference date's month (0-based; 0 is January)
 * @param day        The reference date's day-in-month (1-based)
 * @param dayOfWeek  The reference date's day-of-week (1-based; 1 is Sunday)
 * @param millis     The reference date's milliseconds in day, local standard time
 * @param status     Output param to filled in with a success or an error.
 * @return           The offset in milliseconds to add to GMT to get local time.
 */
U_CAPI int32_t U_EXPORT2
vzone_getOffset(VZone* zone, uint8_t era, int32_t year, int32_t month, int32_t day,
                uint8_t dayOfWeek, int32_t millis, UErrorCode& status);

/**
 * Gets the time zone offset, for current date, modified in case of
 * daylight savings. This is the offset to add *to* UTC to get local time.
 *
 * <p>Note: Don't call this method. Instead, call the getOffset(UDate...) overload,
 * which returns both the raw and the DST offset for a given time. This method
 * is retained only for backward compatibility.
 *
 * @param zone, the vzone to use
 * @param era        The reference date's era
 * @param year       The reference date's year
 * @param month      The reference date's month (0-based; 0 is January)
 * @param day        The reference date's day-in-month (1-based)
 * @param dayOfWeek  The reference date's day-of-week (1-based; 1 is Sunday)
 * @param millis     The reference date's milliseconds in day, local standard time
 * @param monthLength The length of the given month in days.
 * @param status     Output param to filled in with a success or an error.
 * @return           The offset in milliseconds to add to GMT to get local time.
 */
U_CAPI int32_t U_EXPORT2
vzone_getOffset2(VZone* zone, uint8_t era, int32_t year, int32_t month, int32_t day,
                uint8_t dayOfWeek, int32_t millis,
                int32_t monthLength, UErrorCode& status);

/**
 * Returns the time zone raw and GMT offset for the given moment
 * in time.  Upon return, local-millis = GMT-millis + rawOffset +
 * dstOffset.  All computations are performed in the proleptic
 * Gregorian calendar.  The default implementation in the TimeZone
 * class delegates to the 8-argument getOffset().
 *
 * @param zone, the vzone to use
 * @param date moment in time for which to return offsets, in
 * units of milliseconds from January 1, 1970 0:00 GMT, either GMT
 * time or local wall time, depending on `local'.
 * @param local if true, `date' is local wall time; otherwise it
 * is in GMT time.
 * @param rawOffset output parameter to receive the raw offset, that
 * is, the offset not including DST adjustments
 * @param dstOffset output parameter to receive the DST offset,
 * that is, the offset to be added to `rawOffset' to obtain the
 * total offset between local and GMT time. If DST is not in
 * effect, this value is zero; otherwise it is a positive value,
 * typically one hour.
 * @param ec input-output error code
 */
U_CAPI void U_EXPORT2
vzone_getOffset3(VZone* zone, UDate date, UBool local, int32_t& rawOffset,
                int32_t& dstOffset, UErrorCode& ec);

/**
 * Sets the TimeZone's raw GMT offset (i.e., the number of milliseconds to add
 * to GMT to get local time, before taking daylight savings time into account).
 *
 * @param zone, the vzone to use
 * @param offsetMillis  The new raw GMT offset for this time zone.
 */
U_CAPI void U_EXPORT2
vzone_setRawOffset(VZone* zone, int32_t offsetMillis);

/**
 * Returns the TimeZone's raw GMT offset (i.e., the number of milliseconds to add
 * to GMT to get local time, before taking daylight savings time into account).
 *
 * @param zone, the vzone to use
 * @return   The TimeZone's raw GMT offset.
 */
U_CAPI int32_t U_EXPORT2
vzone_getRawOffset(VZone* zone);

/**
 * Queries if this time zone uses daylight savings time.
 * @param zone, the vzone to use
 * @return true if this time zone uses daylight savings time,
 * false, otherwise.
 */
U_CAPI UBool U_EXPORT2
vzone_useDaylightTime(VZone* zone);

/**
 * Queries if the given date is in daylight savings time in
 * this time zone.
 * This method is wasteful since it creates a new GregorianCalendar and
 * deletes it each time it is called. This is a deprecated method
 * and provided only for Java compatibility.
 *
 * @param zone, the vzone to use
 * @param date the given UDate.
 * @param status Output param filled in with success/error code.
 * @return true if the given date is in daylight savings time,
 * false, otherwise.
 */
U_INTERNAL UBool U_EXPORT2
vzone_inDaylightTime(VZone* zone, UDate date, UErrorCode& status);

/**
 * Returns true if this zone has the same rule and offset as another zone.
 * That is, if this zone differs only in ID, if at all.
 * @param zone, the vzone to use
 * @param other the <code>TimeZone</code> object to be compared with
 * @return true if the given zone is the same as this one,
 * with the possible exception of the ID
 */
U_CAPI UBool U_EXPORT2
vzone_hasSameRules(VZone* zone, const VZone* other);

/**
 * Gets the first time zone transition after the base time.
 * @param zone, the vzone to use
 * @param base      The base time.
 * @param inclusive Whether the base time is inclusive or not.
 * @param result    Receives the first transition after the base time.
 * @return  TRUE if the transition is found.
 */
U_CAPI UBool U_EXPORT2
vzone_getNextTransition(VZone* zone, UDate base, UBool inclusive, ZTrans* result);

/**
 * Gets the most recent time zone transition before the base time.
 * @param zone, the vzone to use
 * @param base      The base time.
 * @param inclusive Whether the base time is inclusive or not.
 * @param result    Receives the most recent transition before the base time.
 * @return  TRUE if the transition is found.
 */
U_CAPI UBool U_EXPORT2
vzone_getPreviousTransition(VZone* zone, UDate base, UBool inclusive, ZTrans* result);

/**
 * Returns the number of <code>TimeZoneRule</code>s which represents time transitions,
 * for this time zone, that is, all <code>TimeZoneRule</code>s for this time zone except
 * <code>InitialTimeZoneRule</code>.  The return value range is 0 or any positive value.
 * @param zone, the vzone to use     
 * @param status    Receives error status code.
 * @return The number of <code>TimeZoneRule</code>s representing time transitions.
 */
U_CAPI int32_t U_EXPORT2
vzone_countTransitionRules(VZone* zone, UErrorCode& status);

/**
 * Return the class ID for this class. This is useful only for comparing to
 * a return value from getDynamicClassID(). For example:
 * <pre>
 * .   Base* polymorphic_pointer = createPolymorphicObject();
 * .   if (polymorphic_pointer->getDynamicClassID() ==
 * .       erived::getStaticClassID()) ...
 * </pre>
 * @param zone, the vzone to use
 * @return          The class ID for all objects of this class.
 */
U_CAPI UClassID U_EXPORT2
vzone_getStaticClassID(VZone* zone);

/**
 * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
 * method is to implement a simple version of RTTI, since not all C++
 * compilers support genuine RTTI. Polymorphic operator==() and clone()
 * methods call this method.
 *
 * @param zone, the vzone to use
 * @return          The class ID for this object. All objects of a
 *                  given class have the same class ID.  Objects of
 *                  other classes have different class IDs.
 */
U_CAPI UClassID U_EXPORT2
vzone_getDynamicClassID(VZone* zone);

#endif // __VZONE_H

#endif
