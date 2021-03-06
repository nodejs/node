// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *****************************************************************************
 * Copyright (C) 2007-2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 *****************************************************************************
 *
 * File CHNSECAL.H
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   9/18/2007  ajmacher         ported from java ChineseCalendar
 *****************************************************************************
 */

#ifndef CHNSECAL_H
#define CHNSECAL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"
#include "unicode/timezone.h"

U_NAMESPACE_BEGIN

/**
 * <code>ChineseCalendar</code> is a concrete subclass of {@link Calendar}
 * that implements a traditional Chinese calendar.  The traditional Chinese
 * calendar is a lunisolar calendar: Each month starts on a new moon, and
 * the months are numbered according to solar events, specifically, to
 * guarantee that month 11 always contains the winter solstice.  In order
 * to accomplish this, leap months are inserted in certain years.  Leap
 * months are numbered the same as the month they follow.  The decision of
 * which month is a leap month depends on the relative movements of the sun
 * and moon.
 *
 * <p>This class defines one addition field beyond those defined by
 * <code>Calendar</code>: The <code>IS_LEAP_MONTH</code> field takes the
 * value of 0 for normal months, or 1 for leap months.
 *
 * <p>All astronomical computations are performed with respect to a time
 * zone of GMT+8:00 and a longitude of 120 degrees east.  Although some
 * calendars implement a historically more accurate convention of using
 * Beijing's local longitude (116 degrees 25 minutes east) and time zone
 * (GMT+7:45:40) for dates before 1929, we do not implement this here.
 *
 * <p>Years are counted in two different ways in the Chinese calendar.  The
 * first method is by sequential numbering from the 61st year of the reign
 * of Huang Di, 2637 BCE, which is designated year 1 on the Chinese
 * calendar.  The second method uses 60-year cycles from the same starting
 * point, which is designated year 1 of cycle 1.  In this class, the
 * <code>EXTENDED_YEAR</code> field contains the sequential year count.
 * The <code>ERA</code> field contains the cycle number, and the
 * <code>YEAR</code> field contains the year of the cycle, a value between
 * 1 and 60.
 *
 * <p>There is some variation in what is considered the starting point of
 * the calendar, with some sources starting in the first year of the reign
 * of Huang Di, rather than the 61st.  This gives continuous year numbers
 * 60 years greater and cycle numbers one greater than what this class
 * implements.
 *
 * <p>Because <code>ChineseCalendar</code> defines an additional field and
 * redefines the way the <code>ERA</code> field is used, it requires a new
 * format class, <code>ChineseDateFormat</code>.  As always, use the
 * methods <code>DateFormat.getXxxInstance(Calendar cal,...)</code> to
 * obtain a formatter for this calendar.
 *
 * <p>References:<ul>
 *
 * <li>Dershowitz and Reingold, <i>Calendrical Calculations</i>,
 * Cambridge University Press, 1997</li>
 *
 * <li>Helmer Aslaksen's
 * <a href="http://www.math.nus.edu.sg/aslaksen/calendar/chinese.shtml">
 * Chinese Calendar page</a></li>
 *
 * <li>The <a href="http://www.tondering.dk/claus/calendar.html">
 * Calendar FAQ</a></li>
 *
 * </ul>
 *
 * <p>
 * This class should only be subclassed to implement variants of the Chinese lunar calendar.</p>
 * <p>
 * ChineseCalendar usually should be instantiated using
 * {@link com.ibm.icu.util.Calendar#getInstance(ULocale)} passing in a <code>ULocale</code>
 * with the tag <code>"@calendar=chinese"</code>.</p>
 *
 * @see com.ibm.icu.text.ChineseDateFormat
 * @see com.ibm.icu.util.Calendar
 * @author Alan Liu
 * @internal
 */
class U_I18N_API ChineseCalendar : public Calendar {
 public:
  //-------------------------------------------------------------------------
  // Constructors...
  //-------------------------------------------------------------------------

  /**
   * Constructs a ChineseCalendar based on the current time in the default time zone
   * with the given locale.
   *
   * @param aLocale  The given locale.
   * @param success  Indicates the status of ChineseCalendar object construction.
   *                 Returns U_ZERO_ERROR if constructed successfully.
   * @internal
   */
  ChineseCalendar(const Locale& aLocale, UErrorCode &success);

 protected:

   /**
   * Constructs a ChineseCalendar based on the current time in the default time zone
   * with the given locale, using the specified epoch year and time zone for
   * astronomical calculations.
   *
   * @param aLocale         The given locale.
   * @param epochYear       The epoch year to use for calculation.
   * @param zoneAstroCalc   The TimeZone to use for astronomical calculations. If null,
   *                        will be set appropriately for Chinese calendar (UTC + 8:00).
   * @param success         Indicates the status of ChineseCalendar object construction;
   *                        if successful, will not be changed to an error value.
   * @internal
   */
  ChineseCalendar(const Locale& aLocale, int32_t epochYear, const TimeZone* zoneAstroCalc, UErrorCode &success);

 public:
  /**
   * Copy Constructor
   * @internal
   */
  ChineseCalendar(const ChineseCalendar& other);

  /**
   * Destructor.
   * @internal
   */
  virtual ~ChineseCalendar();

  // clone
  virtual ChineseCalendar* clone() const;

 private:

  //-------------------------------------------------------------------------
  // Internal data....
  //-------------------------------------------------------------------------

  UBool isLeapYear;
  int32_t fEpochYear;   // Start year of this Chinese calendar instance.
  const TimeZone* fZoneAstroCalc;   // Zone used for the astronomical calculation
                                    // of this Chinese calendar instance.

  //----------------------------------------------------------------------
  // Calendar framework
  //----------------------------------------------------------------------

 protected:
  virtual int32_t handleGetLimit(UCalendarDateFields field, ELimitType limitType) const;
  virtual int32_t handleGetMonthLength(int32_t extendedYear, int32_t month) const;
  virtual int32_t handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth) const;
  virtual int32_t handleGetExtendedYear();
  virtual void handleComputeFields(int32_t julianDay, UErrorCode &status);
  virtual const UFieldResolutionTable* getFieldResolutionTable() const;

 public:
  virtual void add(UCalendarDateFields field, int32_t amount, UErrorCode &status);
  virtual void add(EDateFields field, int32_t amount, UErrorCode &status);
  virtual void roll(UCalendarDateFields field, int32_t amount, UErrorCode &status);
  virtual void roll(EDateFields field, int32_t amount, UErrorCode &status);

  //----------------------------------------------------------------------
  // Internal methods & astronomical calculations
  //----------------------------------------------------------------------

 private:

  static const UFieldResolutionTable CHINESE_DATE_PRECEDENCE[];

  double daysToMillis(double days) const;
  double millisToDays(double millis) const;
  virtual int32_t winterSolstice(int32_t gyear) const;
  virtual int32_t newMoonNear(double days, UBool after) const;
  virtual int32_t synodicMonthsBetween(int32_t day1, int32_t day2) const;
  virtual int32_t majorSolarTerm(int32_t days) const;
  virtual UBool hasNoMajorSolarTerm(int32_t newMoon) const;
  virtual UBool isLeapMonthBetween(int32_t newMoon1, int32_t newMoon2) const;
  virtual void computeChineseFields(int32_t days, int32_t gyear,
                 int32_t gmonth, UBool setAllFields);
  virtual int32_t newYear(int32_t gyear) const;
  virtual void offsetMonth(int32_t newMoon, int32_t dom, int32_t delta);
  const TimeZone* getChineseCalZoneAstroCalc(void) const;

  // UObject stuff
 public:
  /**
   * @return   The class ID for this object. All objects of a given class have the
   *           same class ID. Objects of other classes have different class IDs.
   * @internal
   */
  virtual UClassID getDynamicClassID(void) const;

  /**
   * Return the class ID for this class. This is useful only for comparing to a return
   * value from getDynamicClassID(). For example:
   *
   *      Base* polymorphic_pointer = createPolymorphicObject();
   *      if (polymorphic_pointer->getDynamicClassID() ==
   *          Derived::getStaticClassID()) ...
   *
   * @return   The class ID for all objects of this class.
   * @internal
   */
  static UClassID U_EXPORT2 getStaticClassID(void);

  /**
   * return the calendar type, "chinese".
   *
   * @return calendar type
   * @internal
   */
  virtual const char * getType() const;


 protected:
  /**
   * (Overrides Calendar) Return true if the current date for this Calendar is in
   * Daylight Savings Time. Recognizes DST_OFFSET, if it is set.
   *
   * @param status Fill-in parameter which receives the status of this operation.
   * @return   True if the current date for this Calendar is in Daylight Savings Time,
   *           false, otherwise.
   * @internal
   */
  virtual UBool inDaylightTime(UErrorCode& status) const;


  /**
   * Returns true because the Islamic Calendar does have a default century
   * @internal
   */
  virtual UBool haveDefaultCentury() const;

  /**
   * Returns the date of the start of the default century
   * @return start of century - in milliseconds since epoch, 1970
   * @internal
   */
  virtual UDate defaultCenturyStart() const;

  /**
   * Returns the year in which the default century begins
   * @internal
   */
  virtual int32_t defaultCenturyStartYear() const;

 private: // default century stuff.

  /**
   * Returns the beginning date of the 100-year window that dates
   * with 2-digit years are considered to fall within.
   */
  UDate         internalGetDefaultCenturyStart(void) const;

  /**
   * Returns the first year of the 100-year window that dates with
   * 2-digit years are considered to fall within.
   */
  int32_t          internalGetDefaultCenturyStartYear(void) const;

  ChineseCalendar(); // default constructor not implemented
};

U_NAMESPACE_END

#endif
#endif
