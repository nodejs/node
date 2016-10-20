// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/************************************************************************
 * Copyright (C) 1996-2012, International Business Machines Corporation
 * and others. All Rights Reserved.
 ************************************************************************
 *  2003-nov-07   srl       Port from Java
 */

#include "astro.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"
#include <math.h>
#include <float.h>
#include "unicode/putil.h"
#include "uhash.h"
#include "umutex.h"
#include "ucln_in.h"
#include "putilimp.h"
#include <stdio.h>  // for toString()

#if defined (PI)
#undef PI
#endif

#ifdef U_DEBUG_ASTRO
# include "uresimp.h" // for debugging

static void debug_astro_loc(const char *f, int32_t l)
{
  fprintf(stderr, "%s:%d: ", f, l);
}

static void debug_astro_msg(const char *pat, ...)
{
  va_list ap;
  va_start(ap, pat);
  vfprintf(stderr, pat, ap);
  fflush(stderr);
}
#include "unicode/datefmt.h"
#include "unicode/ustring.h"
static const char * debug_astro_date(UDate d) {
  static char gStrBuf[1024];
  static DateFormat *df = NULL;
  if(df == NULL) {
    df = DateFormat::createDateTimeInstance(DateFormat::MEDIUM, DateFormat::MEDIUM, Locale::getUS());
    df->adoptTimeZone(TimeZone::getGMT()->clone());
  }
  UnicodeString str;
  df->format(d,str);
  u_austrncpy(gStrBuf,str.getTerminatedBuffer(),sizeof(gStrBuf)-1);
  return gStrBuf;
}

// must use double parens, i.e.:  U_DEBUG_ASTRO_MSG(("four is: %d",4));
#define U_DEBUG_ASTRO_MSG(x) {debug_astro_loc(__FILE__,__LINE__);debug_astro_msg x;}
#else
#define U_DEBUG_ASTRO_MSG(x)
#endif

static inline UBool isINVALID(double d) {
  return(uprv_isNaN(d));
}

static UMutex ccLock = U_MUTEX_INITIALIZER;

U_CDECL_BEGIN
static UBool calendar_astro_cleanup(void) {
  return TRUE;
}
U_CDECL_END

U_NAMESPACE_BEGIN

/**
 * The number of standard hours in one sidereal day.
 * Approximately 24.93.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define SIDEREAL_DAY (23.93446960027)

/**
 * The number of sidereal hours in one mean solar day.
 * Approximately 24.07.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define SOLAR_DAY  (24.065709816)

/**
 * The average number of solar days from one new moon to the next.  This is the time
 * it takes for the moon to return the same ecliptic longitude as the sun.
 * It is longer than the sidereal month because the sun's longitude increases
 * during the year due to the revolution of the earth around the sun.
 * Approximately 29.53.
 *
 * @see #SIDEREAL_MONTH
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
const double CalendarAstronomer::SYNODIC_MONTH  = 29.530588853;

/**
 * The average number of days it takes
 * for the moon to return to the same ecliptic longitude relative to the
 * stellar background.  This is referred to as the sidereal month.
 * It is shorter than the synodic month due to
 * the revolution of the earth around the sun.
 * Approximately 27.32.
 *
 * @see #SYNODIC_MONTH
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define SIDEREAL_MONTH  27.32166

/**
 * The average number number of days between successive vernal equinoxes.
 * Due to the precession of the earth's
 * axis, this is not precisely the same as the sidereal year.
 * Approximately 365.24
 *
 * @see #SIDEREAL_YEAR
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define TROPICAL_YEAR  365.242191

/**
 * The average number of days it takes
 * for the sun to return to the same position against the fixed stellar
 * background.  This is the duration of one orbit of the earth about the sun
 * as it would appear to an outside observer.
 * Due to the precession of the earth's
 * axis, this is not precisely the same as the tropical year.
 * Approximately 365.25.
 *
 * @see #TROPICAL_YEAR
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define SIDEREAL_YEAR  365.25636

//-------------------------------------------------------------------------
// Time-related constants
//-------------------------------------------------------------------------

/**
 * The number of milliseconds in one second.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define SECOND_MS  U_MILLIS_PER_SECOND

/**
 * The number of milliseconds in one minute.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define MINUTE_MS  U_MILLIS_PER_MINUTE

/**
 * The number of milliseconds in one hour.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define HOUR_MS   U_MILLIS_PER_HOUR

/**
 * The number of milliseconds in one day.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define DAY_MS U_MILLIS_PER_DAY

/**
 * The start of the julian day numbering scheme used by astronomers, which
 * is 1/1/4713 BC (Julian), 12:00 GMT.  This is given as the number of milliseconds
 * since 1/1/1970 AD (Gregorian), a negative number.
 * Note that julian day numbers and
 * the Julian calendar are <em>not</em> the same thing.  Also note that
 * julian days start at <em>noon</em>, not midnight.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
#define JULIAN_EPOCH_MS  -210866760000000.0


/**
 * Milliseconds value for 0.0 January 2000 AD.
 */
#define EPOCH_2000_MS  946598400000.0

//-------------------------------------------------------------------------
// Assorted private data used for conversions
//-------------------------------------------------------------------------

// My own copies of these so compilers are more likely to optimize them away
const double CalendarAstronomer::PI = 3.14159265358979323846;

#define CalendarAstronomer_PI2  (CalendarAstronomer::PI*2.0)
#define RAD_HOUR  ( 12 / CalendarAstronomer::PI )     // radians -> hours
#define DEG_RAD ( CalendarAstronomer::PI / 180 )      // degrees -> radians
#define RAD_DEG  ( 180 / CalendarAstronomer::PI )     // radians -> degrees

/***
 * Given 'value', add or subtract 'range' until 0 <= 'value' < range.
 * The modulus operator.
 */
inline static double normalize(double value, double range)  {
    return value - range * ClockMath::floorDivide(value, range);
}

/**
 * Normalize an angle so that it's in the range 0 - 2pi.
 * For positive angles this is just (angle % 2pi), but the Java
 * mod operator doesn't work that way for negative numbers....
 */
inline static double norm2PI(double angle)  {
    return normalize(angle, CalendarAstronomer::PI * 2.0);
}

/**
 * Normalize an angle into the range -PI - PI
 */
inline static  double normPI(double angle)  {
    return normalize(angle + CalendarAstronomer::PI, CalendarAstronomer::PI * 2.0) - CalendarAstronomer::PI;
}

//-------------------------------------------------------------------------
// Constructors
//-------------------------------------------------------------------------

/**
 * Construct a new <code>CalendarAstronomer</code> object that is initialized to
 * the current date and time.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::CalendarAstronomer():
  fTime(Calendar::getNow()), fLongitude(0.0), fLatitude(0.0), fGmtOffset(0.0), moonPosition(0,0), moonPositionSet(FALSE) {
  clearCache();
}

/**
 * Construct a new <code>CalendarAstronomer</code> object that is initialized to
 * the specified date and time.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::CalendarAstronomer(UDate d): fTime(d), fLongitude(0.0), fLatitude(0.0), fGmtOffset(0.0), moonPosition(0,0), moonPositionSet(FALSE) {
  clearCache();
}

/**
 * Construct a new <code>CalendarAstronomer</code> object with the given
 * latitude and longitude.  The object's time is set to the current
 * date and time.
 * <p>
 * @param longitude The desired longitude, in <em>degrees</em> east of
 *                  the Greenwich meridian.
 *
 * @param latitude  The desired latitude, in <em>degrees</em>.  Positive
 *                  values signify North, negative South.
 *
 * @see java.util.Date#getTime()
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::CalendarAstronomer(double longitude, double latitude) :
  fTime(Calendar::getNow()), moonPosition(0,0), moonPositionSet(FALSE) {
  fLongitude = normPI(longitude * (double)DEG_RAD);
  fLatitude  = normPI(latitude  * (double)DEG_RAD);
  fGmtOffset = (double)(fLongitude * 24. * (double)HOUR_MS / (double)CalendarAstronomer_PI2);
  clearCache();
}

CalendarAstronomer::~CalendarAstronomer()
{
}

//-------------------------------------------------------------------------
// Time and date getters and setters
//-------------------------------------------------------------------------

/**
 * Set the current date and time of this <code>CalendarAstronomer</code> object.  All
 * astronomical calculations are performed based on this time setting.
 *
 * @param aTime the date and time, expressed as the number of milliseconds since
 *              1/1/1970 0:00 GMT (Gregorian).
 *
 * @see #setDate
 * @see #getTime
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
void CalendarAstronomer::setTime(UDate aTime) {
    fTime = aTime;
    U_DEBUG_ASTRO_MSG(("setTime(%.1lf, %sL)\n", aTime, debug_astro_date(aTime+fGmtOffset)));
    clearCache();
}

/**
 * Set the current date and time of this <code>CalendarAstronomer</code> object.  All
 * astronomical calculations are performed based on this time setting.
 *
 * @param jdn   the desired time, expressed as a "julian day number",
 *              which is the number of elapsed days since
 *              1/1/4713 BC (Julian), 12:00 GMT.  Note that julian day
 *              numbers start at <em>noon</em>.  To get the jdn for
 *              the corresponding midnight, subtract 0.5.
 *
 * @see #getJulianDay
 * @see #JULIAN_EPOCH_MS
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
void CalendarAstronomer::setJulianDay(double jdn) {
    fTime = (double)(jdn * DAY_MS) + JULIAN_EPOCH_MS;
    clearCache();
    julianDay = jdn;
}

/**
 * Get the current time of this <code>CalendarAstronomer</code> object,
 * represented as the number of milliseconds since
 * 1/1/1970 AD 0:00 GMT (Gregorian).
 *
 * @see #setTime
 * @see #getDate
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
UDate CalendarAstronomer::getTime() {
    return fTime;
}

/**
 * Get the current time of this <code>CalendarAstronomer</code> object,
 * expressed as a "julian day number", which is the number of elapsed
 * days since 1/1/4713 BC (Julian), 12:00 GMT.
 *
 * @see #setJulianDay
 * @see #JULIAN_EPOCH_MS
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::getJulianDay() {
    if (isINVALID(julianDay)) {
        julianDay = (fTime - (double)JULIAN_EPOCH_MS) / (double)DAY_MS;
    }
    return julianDay;
}

/**
 * Return this object's time expressed in julian centuries:
 * the number of centuries after 1/1/1900 AD, 12:00 GMT
 *
 * @see #getJulianDay
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::getJulianCentury() {
    if (isINVALID(julianCentury)) {
        julianCentury = (getJulianDay() - 2415020.0) / 36525.0;
    }
    return julianCentury;
}

/**
 * Returns the current Greenwich sidereal time, measured in hours
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::getGreenwichSidereal() {
    if (isINVALID(siderealTime)) {
        // See page 86 of "Practial Astronomy with your Calculator",
        // by Peter Duffet-Smith, for details on the algorithm.

        double UT = normalize(fTime/(double)HOUR_MS, 24.);

        siderealTime = normalize(getSiderealOffset() + UT*1.002737909, 24.);
    }
    return siderealTime;
}

double CalendarAstronomer::getSiderealOffset() {
    if (isINVALID(siderealT0)) {
        double JD  = uprv_floor(getJulianDay() - 0.5) + 0.5;
        double S   = JD - 2451545.0;
        double T   = S / 36525.0;
        siderealT0 = normalize(6.697374558 + 2400.051336*T + 0.000025862*T*T, 24);
    }
    return siderealT0;
}

/**
 * Returns the current local sidereal time, measured in hours
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::getLocalSidereal() {
    return normalize(getGreenwichSidereal() + (fGmtOffset/(double)HOUR_MS), 24.);
}

/**
 * Converts local sidereal time to Universal Time.
 *
 * @param lst   The Local Sidereal Time, in hours since sidereal midnight
 *              on this object's current date.
 *
 * @return      The corresponding Universal Time, in milliseconds since
 *              1 Jan 1970, GMT.
 */
double CalendarAstronomer::lstToUT(double lst) {
    // Convert to local mean time
    double lt = normalize((lst - getSiderealOffset()) * 0.9972695663, 24);

    // Then find local midnight on this day
    double base = (DAY_MS * ClockMath::floorDivide(fTime + fGmtOffset,(double)DAY_MS)) - fGmtOffset;

    //out("    lt  =" + lt + " hours");
    //out("    base=" + new Date(base));

    return base + (long)(lt * HOUR_MS);
}


//-------------------------------------------------------------------------
// Coordinate transformations, all based on the current time of this object
//-------------------------------------------------------------------------

/**
 * Convert from ecliptic to equatorial coordinates.
 *
 * @param ecliptic  A point in the sky in ecliptic coordinates.
 * @return          The corresponding point in equatorial coordinates.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::Equatorial& CalendarAstronomer::eclipticToEquatorial(CalendarAstronomer::Equatorial& result, const CalendarAstronomer::Ecliptic& ecliptic)
{
    return eclipticToEquatorial(result, ecliptic.longitude, ecliptic.latitude);
}

/**
 * Convert from ecliptic to equatorial coordinates.
 *
 * @param eclipLong     The ecliptic longitude
 * @param eclipLat      The ecliptic latitude
 *
 * @return              The corresponding point in equatorial coordinates.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::Equatorial& CalendarAstronomer::eclipticToEquatorial(CalendarAstronomer::Equatorial& result, double eclipLong, double eclipLat)
{
    // See page 42 of "Practial Astronomy with your Calculator",
    // by Peter Duffet-Smith, for details on the algorithm.

    double obliq = eclipticObliquity();
    double sinE = ::sin(obliq);
    double cosE = cos(obliq);

    double sinL = ::sin(eclipLong);
    double cosL = cos(eclipLong);

    double sinB = ::sin(eclipLat);
    double cosB = cos(eclipLat);
    double tanB = tan(eclipLat);

    result.set(atan2(sinL*cosE - tanB*sinE, cosL),
        asin(sinB*cosE + cosB*sinE*sinL) );
    return result;
}

/**
 * Convert from ecliptic longitude to equatorial coordinates.
 *
 * @param eclipLong     The ecliptic longitude
 *
 * @return              The corresponding point in equatorial coordinates.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::Equatorial& CalendarAstronomer::eclipticToEquatorial(CalendarAstronomer::Equatorial& result, double eclipLong)
{
    return eclipticToEquatorial(result, eclipLong, 0);  // TODO: optimize
}

/**
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::Horizon& CalendarAstronomer::eclipticToHorizon(CalendarAstronomer::Horizon& result, double eclipLong)
{
    Equatorial equatorial;
    eclipticToEquatorial(equatorial, eclipLong);

    double H = getLocalSidereal()*CalendarAstronomer::PI/12 - equatorial.ascension;     // Hour-angle

    double sinH = ::sin(H);
    double cosH = cos(H);
    double sinD = ::sin(equatorial.declination);
    double cosD = cos(equatorial.declination);
    double sinL = ::sin(fLatitude);
    double cosL = cos(fLatitude);

    double altitude = asin(sinD*sinL + cosD*cosL*cosH);
    double azimuth  = atan2(-cosD*cosL*sinH, sinD - sinL * ::sin(altitude));

    result.set(azimuth, altitude);
    return result;
}


//-------------------------------------------------------------------------
// The Sun
//-------------------------------------------------------------------------

//
// Parameters of the Sun's orbit as of the epoch Jan 0.0 1990
// Angles are in radians (after multiplying by CalendarAstronomer::PI/180)
//
#define JD_EPOCH  2447891.5 // Julian day of epoch

#define SUN_ETA_G    (279.403303 * CalendarAstronomer::PI/180) // Ecliptic longitude at epoch
#define SUN_OMEGA_G  (282.768422 * CalendarAstronomer::PI/180) // Ecliptic longitude of perigee
#define SUN_E         0.016713          // Eccentricity of orbit
//double sunR0        1.495585e8        // Semi-major axis in KM
//double sunTheta0    (0.533128 * CalendarAstronomer::PI/180) // Angular diameter at R0

// The following three methods, which compute the sun parameters
// given above for an arbitrary epoch (whatever time the object is
// set to), make only a small difference as compared to using the
// above constants.  E.g., Sunset times might differ by ~12
// seconds.  Furthermore, the eta-g computation is befuddled by
// Duffet-Smith's incorrect coefficients (p.86).  I've corrected
// the first-order coefficient but the others may be off too - no
// way of knowing without consulting another source.

//  /**
//   * Return the sun's ecliptic longitude at perigee for the current time.
//   * See Duffett-Smith, p. 86.
//   * @return radians
//   */
//  private double getSunOmegaG() {
//      double T = getJulianCentury();
//      return (281.2208444 + (1.719175 + 0.000452778*T)*T) * DEG_RAD;
//  }

//  /**
//   * Return the sun's ecliptic longitude for the current time.
//   * See Duffett-Smith, p. 86.
//   * @return radians
//   */
//  private double getSunEtaG() {
//      double T = getJulianCentury();
//      //return (279.6966778 + (36000.76892 + 0.0003025*T)*T) * DEG_RAD;
//      //
//      // The above line is from Duffett-Smith, and yields manifestly wrong
//      // results.  The below constant is derived empirically to match the
//      // constant he gives for the 1990 EPOCH.
//      //
//      return (279.6966778 + (-0.3262541582718024 + 0.0003025*T)*T) * DEG_RAD;
//  }

//  /**
//   * Return the sun's eccentricity of orbit for the current time.
//   * See Duffett-Smith, p. 86.
//   * @return double
//   */
//  private double getSunE() {
//      double T = getJulianCentury();
//      return 0.01675104 - (0.0000418 + 0.000000126*T)*T;
//  }

/**
 * Find the "true anomaly" (longitude) of an object from
 * its mean anomaly and the eccentricity of its orbit.  This uses
 * an iterative solution to Kepler's equation.
 *
 * @param meanAnomaly   The object's longitude calculated as if it were in
 *                      a regular, circular orbit, measured in radians
 *                      from the point of perigee.
 *
 * @param eccentricity  The eccentricity of the orbit
 *
 * @return The true anomaly (longitude) measured in radians
 */
static double trueAnomaly(double meanAnomaly, double eccentricity)
{
    // First, solve Kepler's equation iteratively
    // Duffett-Smith, p.90
    double delta;
    double E = meanAnomaly;
    do {
        delta = E - eccentricity * ::sin(E) - meanAnomaly;
        E = E - delta / (1 - eccentricity * ::cos(E));
    }
    while (uprv_fabs(delta) > 1e-5); // epsilon = 1e-5 rad

    return 2.0 * ::atan( ::tan(E/2) * ::sqrt( (1+eccentricity)
                                             /(1-eccentricity) ) );
}

/**
 * The longitude of the sun at the time specified by this object.
 * The longitude is measured in radians along the ecliptic
 * from the "first point of Aries," the point at which the ecliptic
 * crosses the earth's equatorial plane at the vernal equinox.
 * <p>
 * Currently, this method uses an approximation of the two-body Kepler's
 * equation for the earth and the sun.  It does not take into account the
 * perturbations caused by the other planets, the moon, etc.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::getSunLongitude()
{
    // See page 86 of "Practial Astronomy with your Calculator",
    // by Peter Duffet-Smith, for details on the algorithm.

    if (isINVALID(sunLongitude)) {
        getSunLongitude(getJulianDay(), sunLongitude, meanAnomalySun);
    }
    return sunLongitude;
}

/**
 * TODO Make this public when the entire class is package-private.
 */
/*public*/ void CalendarAstronomer::getSunLongitude(double jDay, double &longitude, double &meanAnomaly)
{
    // See page 86 of "Practial Astronomy with your Calculator",
    // by Peter Duffet-Smith, for details on the algorithm.

    double day = jDay - JD_EPOCH;       // Days since epoch

    // Find the angular distance the sun in a fictitious
    // circular orbit has travelled since the epoch.
    double epochAngle = norm2PI(CalendarAstronomer_PI2/TROPICAL_YEAR*day);

    // The epoch wasn't at the sun's perigee; find the angular distance
    // since perigee, which is called the "mean anomaly"
    meanAnomaly = norm2PI(epochAngle + SUN_ETA_G - SUN_OMEGA_G);

    // Now find the "true anomaly", e.g. the real solar longitude
    // by solving Kepler's equation for an elliptical orbit
    // NOTE: The 3rd ed. of the book lists omega_g and eta_g in different
    // equations; omega_g is to be correct.
    longitude =  norm2PI(trueAnomaly(meanAnomaly, SUN_E) + SUN_OMEGA_G);
}

/**
 * The position of the sun at this object's current date and time,
 * in equatorial coordinates.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
CalendarAstronomer::Equatorial& CalendarAstronomer::getSunPosition(CalendarAstronomer::Equatorial& result) {
    return eclipticToEquatorial(result, getSunLongitude(), 0);
}


/**
 * Constant representing the vernal equinox.
 * For use with {@link #getSunTime getSunTime}.
 * Note: In this case, "vernal" refers to the northern hemisphere's seasons.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
/*double CalendarAstronomer::VERNAL_EQUINOX() {
  return 0;
}*/

/**
 * Constant representing the summer solstice.
 * For use with {@link #getSunTime getSunTime}.
 * Note: In this case, "summer" refers to the northern hemisphere's seasons.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::SUMMER_SOLSTICE() {
    return  (CalendarAstronomer::PI/2);
}

/**
 * Constant representing the autumnal equinox.
 * For use with {@link #getSunTime getSunTime}.
 * Note: In this case, "autumn" refers to the northern hemisphere's seasons.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
/*double CalendarAstronomer::AUTUMN_EQUINOX() {
  return  (CalendarAstronomer::PI);
}*/

/**
 * Constant representing the winter solstice.
 * For use with {@link #getSunTime getSunTime}.
 * Note: In this case, "winter" refers to the northern hemisphere's seasons.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::WINTER_SOLSTICE() {
    return  ((CalendarAstronomer::PI*3)/2);
}

CalendarAstronomer::AngleFunc::~AngleFunc() {}

/**
 * Find the next time at which the sun's ecliptic longitude will have
 * the desired value.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
class SunTimeAngleFunc : public CalendarAstronomer::AngleFunc {
public:
    virtual ~SunTimeAngleFunc();
    virtual double eval(CalendarAstronomer& a) { return a.getSunLongitude(); }
};

SunTimeAngleFunc::~SunTimeAngleFunc() {}

UDate CalendarAstronomer::getSunTime(double desired, UBool next)
{
    SunTimeAngleFunc func;
    return timeOfAngle( func,
                        desired,
                        TROPICAL_YEAR,
                        MINUTE_MS,
                        next);
}

CalendarAstronomer::CoordFunc::~CoordFunc() {}

class RiseSetCoordFunc : public CalendarAstronomer::CoordFunc {
public:
    virtual ~RiseSetCoordFunc();
    virtual void eval(CalendarAstronomer::Equatorial& result, CalendarAstronomer&a) {  a.getSunPosition(result); }
};

RiseSetCoordFunc::~RiseSetCoordFunc() {}

UDate CalendarAstronomer::getSunRiseSet(UBool rise)
{
    UDate t0 = fTime;

    // Make a rough guess: 6am or 6pm local time on the current day
    double noon = ClockMath::floorDivide(fTime + fGmtOffset, (double)DAY_MS)*DAY_MS - fGmtOffset + (12*HOUR_MS);

    U_DEBUG_ASTRO_MSG(("Noon=%.2lf, %sL, gmtoff %.2lf\n", noon, debug_astro_date(noon+fGmtOffset), fGmtOffset));
    setTime(noon +  ((rise ? -6 : 6) * HOUR_MS));
    U_DEBUG_ASTRO_MSG(("added %.2lf ms as a guess,\n", ((rise ? -6. : 6.) * HOUR_MS)));

    RiseSetCoordFunc func;
    double t = riseOrSet(func,
                         rise,
                         .533 * DEG_RAD,        // Angular Diameter
                         34. /60.0 * DEG_RAD,    // Refraction correction
                         MINUTE_MS / 12.);       // Desired accuracy

    setTime(t0);
    return t;
}

// Commented out - currently unused. ICU 2.6, Alan
//    //-------------------------------------------------------------------------
//    // Alternate Sun Rise/Set
//    // See Duffett-Smith p.93
//    //-------------------------------------------------------------------------
//
//    // This yields worse results (as compared to USNO data) than getSunRiseSet().
//    /**
//     * TODO Make this when the entire class is package-private.
//     */
//    /*public*/ long getSunRiseSet2(boolean rise) {
//        // 1. Calculate coordinates of the sun's center for midnight
//        double jd = uprv_floor(getJulianDay() - 0.5) + 0.5;
//        double[] sl = getSunLongitude(jd);//        double lambda1 = sl[0];
//        Equatorial pos1 = eclipticToEquatorial(lambda1, 0);
//
//        // 2. Add ... to lambda to get position 24 hours later
//        double lambda2 = lambda1 + 0.985647*DEG_RAD;
//        Equatorial pos2 = eclipticToEquatorial(lambda2, 0);
//
//        // 3. Calculate LSTs of rising and setting for these two positions
//        double tanL = ::tan(fLatitude);
//        double H = ::acos(-tanL * ::tan(pos1.declination));
//        double lst1r = (CalendarAstronomer_PI2 + pos1.ascension - H) * 24 / CalendarAstronomer_PI2;
//        double lst1s = (pos1.ascension + H) * 24 / CalendarAstronomer_PI2;
//               H = ::acos(-tanL * ::tan(pos2.declination));
//        double lst2r = (CalendarAstronomer_PI2-H + pos2.ascension ) * 24 / CalendarAstronomer_PI2;
//        double lst2s = (H + pos2.ascension ) * 24 / CalendarAstronomer_PI2;
//        if (lst1r > 24) lst1r -= 24;
//        if (lst1s > 24) lst1s -= 24;
//        if (lst2r > 24) lst2r -= 24;
//        if (lst2s > 24) lst2s -= 24;
//
//        // 4. Convert LSTs to GSTs.  If GST1 > GST2, add 24 to GST2.
//        double gst1r = lstToGst(lst1r);
//        double gst1s = lstToGst(lst1s);
//        double gst2r = lstToGst(lst2r);
//        double gst2s = lstToGst(lst2s);
//        if (gst1r > gst2r) gst2r += 24;
//        if (gst1s > gst2s) gst2s += 24;
//
//        // 5. Calculate GST at 0h UT of this date
//        double t00 = utToGst(0);
//
//        // 6. Calculate GST at 0h on the observer's longitude
//        double offset = ::round(fLongitude*12/PI); // p.95 step 6; he _rounds_ to nearest 15 deg.
//        double t00p = t00 - offset*1.002737909;
//        if (t00p < 0) t00p += 24; // do NOT normalize
//
//        // 7. Adjust
//        if (gst1r < t00p) {
//            gst1r += 24;
//            gst2r += 24;
//        }
//        if (gst1s < t00p) {
//            gst1s += 24;
//            gst2s += 24;
//        }
//
//        // 8.
//        double gstr = (24.07*gst1r-t00*(gst2r-gst1r))/(24.07+gst1r-gst2r);
//        double gsts = (24.07*gst1s-t00*(gst2s-gst1s))/(24.07+gst1s-gst2s);
//
//        // 9. Correct for parallax, refraction, and sun's diameter
//        double dec = (pos1.declination + pos2.declination) / 2;
//        double psi = ::acos(sin(fLatitude) / cos(dec));
//        double x = 0.830725 * DEG_RAD; // parallax+refraction+diameter
//        double y = ::asin(sin(x) / ::sin(psi)) * RAD_DEG;
//        double delta_t = 240 * y / cos(dec) / 3600; // hours
//
//        // 10. Add correction to GSTs, subtract from GSTr
//        gstr -= delta_t;
//        gsts += delta_t;
//
//        // 11. Convert GST to UT and then to local civil time
//        double ut = gstToUt(rise ? gstr : gsts);
//        //System.out.println((rise?"rise=":"set=") + ut + ", delta_t=" + delta_t);
//        long midnight = DAY_MS * (time / DAY_MS); // Find UT midnight on this day
//        return midnight + (long) (ut * 3600000);
//    }

// Commented out - currently unused. ICU 2.6, Alan
//    /**
//     * Convert local sidereal time to Greenwich sidereal time.
//     * Section 15.  Duffett-Smith p.21
//     * @param lst in hours (0..24)
//     * @return GST in hours (0..24)
//     */
//    double lstToGst(double lst) {
//        double delta = fLongitude * 24 / CalendarAstronomer_PI2;
//        return normalize(lst - delta, 24);
//    }

// Commented out - currently unused. ICU 2.6, Alan
//    /**
//     * Convert UT to GST on this date.
//     * Section 12.  Duffett-Smith p.17
//     * @param ut in hours
//     * @return GST in hours
//     */
//    double utToGst(double ut) {
//        return normalize(getT0() + ut*1.002737909, 24);
//    }

// Commented out - currently unused. ICU 2.6, Alan
//    /**
//     * Convert GST to UT on this date.
//     * Section 13.  Duffett-Smith p.18
//     * @param gst in hours
//     * @return UT in hours
//     */
//    double gstToUt(double gst) {
//        return normalize(gst - getT0(), 24) * 0.9972695663;
//    }

// Commented out - currently unused. ICU 2.6, Alan
//    double getT0() {
//        // Common computation for UT <=> GST
//
//        // Find JD for 0h UT
//        double jd = uprv_floor(getJulianDay() - 0.5) + 0.5;
//
//        double s = jd - 2451545.0;
//        double t = s / 36525.0;
//        double t0 = 6.697374558 + (2400.051336 + 0.000025862*t)*t;
//        return t0;
//    }

// Commented out - currently unused. ICU 2.6, Alan
//    //-------------------------------------------------------------------------
//    // Alternate Sun Rise/Set
//    // See sci.astro FAQ
//    // http://www.faqs.org/faqs/astronomy/faq/part3/section-5.html
//    //-------------------------------------------------------------------------
//
//    // Note: This method appears to produce inferior accuracy as
//    // compared to getSunRiseSet().
//
//    /**
//     * TODO Make this when the entire class is package-private.
//     */
//    /*public*/ long getSunRiseSet3(boolean rise) {
//
//        // Compute day number for 0.0 Jan 2000 epoch
//        double d = (double)(time - EPOCH_2000_MS) / DAY_MS;
//
//        // Now compute the Local Sidereal Time, LST:
//        //
//        double LST  =  98.9818  +  0.985647352 * d  +  /*UT*15  +  long*/
//            fLongitude*RAD_DEG;
//        //
//        // (east long. positive).  Note that LST is here expressed in degrees,
//        // where 15 degrees corresponds to one hour.  Since LST really is an angle,
//        // it's convenient to use one unit---degrees---throughout.
//
//        //    COMPUTING THE SUN'S POSITION
//        //    ----------------------------
//        //
//        // To be able to compute the Sun's rise/set times, you need to be able to
//        // compute the Sun's position at any time.  First compute the "day
//        // number" d as outlined above, for the desired moment.  Next compute:
//        //
//        double oblecl = 23.4393 - 3.563E-7 * d;
//        //
//        double w  =  282.9404  +  4.70935E-5   * d;
//        double M  =  356.0470  +  0.9856002585 * d;
//        double e  =  0.016709  -  1.151E-9     * d;
//        //
//        // This is the obliquity of the ecliptic, plus some of the elements of
//        // the Sun's apparent orbit (i.e., really the Earth's orbit): w =
//        // argument of perihelion, M = mean anomaly, e = eccentricity.
//        // Semi-major axis is here assumed to be exactly 1.0 (while not strictly
//        // true, this is still an accurate approximation).  Next compute E, the
//        // eccentric anomaly:
//        //
//        double E = M + e*(180/PI) * ::sin(M*DEG_RAD) * ( 1.0 + e*cos(M*DEG_RAD) );
//        //
//        // where E and M are in degrees.  This is it---no further iterations are
//        // needed because we know e has a sufficiently small value.  Next compute
//        // the true anomaly, v, and the distance, r:
//        //
//        /*      r * cos(v)  =  */ double A  =  cos(E*DEG_RAD) - e;
//        /*      r * ::sin(v)  =  */ double B  =  ::sqrt(1 - e*e) * ::sin(E*DEG_RAD);
//        //
//        // and
//        //
//        //      r  =  sqrt( A*A + B*B )
//        double v  =  ::atan2( B, A )*RAD_DEG;
//        //
//        // The Sun's true longitude, slon, can now be computed:
//        //
//        double slon  =  v + w;
//        //
//        // Since the Sun is always at the ecliptic (or at least very very close to
//        // it), we can use simplified formulae to convert slon (the Sun's ecliptic
//        // longitude) to sRA and sDec (the Sun's RA and Dec):
//        //
//        //                   ::sin(slon) * cos(oblecl)
//        //     tan(sRA)  =  -------------------------
//        //            cos(slon)
//        //
//        //     ::sin(sDec) =  ::sin(oblecl) * ::sin(slon)
//        //
//        // As was the case when computing az, the Azimuth, if possible use an
//        // atan2() function to compute sRA.
//
//        double sRA = ::atan2(sin(slon*DEG_RAD) * cos(oblecl*DEG_RAD), cos(slon*DEG_RAD))*RAD_DEG;
//
//        double sin_sDec = ::sin(oblecl*DEG_RAD) * ::sin(slon*DEG_RAD);
//        double sDec = ::asin(sin_sDec)*RAD_DEG;
//
//        //    COMPUTING RISE AND SET TIMES
//        //    ----------------------------
//        //
//        // To compute when an object rises or sets, you must compute when it
//        // passes the meridian and the HA of rise/set.  Then the rise time is
//        // the meridian time minus HA for rise/set, and the set time is the
//        // meridian time plus the HA for rise/set.
//        //
//        // To find the meridian time, compute the Local Sidereal Time at 0h local
//        // time (or 0h UT if you prefer to work in UT) as outlined above---name
//        // that quantity LST0.  The Meridian Time, MT, will now be:
//        //
//        //     MT  =  RA - LST0
//        double MT = normalize(sRA - LST, 360);
//        //
//        // where "RA" is the object's Right Ascension (in degrees!).  If negative,
//        // add 360 deg to MT.  If the object is the Sun, leave the time as it is,
//        // but if it's stellar, multiply MT by 365.2422/366.2422, to convert from
//        // sidereal to solar time.  Now, compute HA for rise/set, name that
//        // quantity HA0:
//        //
//        //                 ::sin(h0)  -  ::sin(lat) * ::sin(Dec)
//        // cos(HA0)  =  ---------------------------------
//        //                      cos(lat) * cos(Dec)
//        //
//        // where h0 is the altitude selected to represent rise/set.  For a purely
//        // mathematical horizon, set h0 = 0 and simplify to:
//        //
//        //    cos(HA0)  =  - tan(lat) * tan(Dec)
//        //
//        // If you want to account for refraction on the atmosphere, set h0 = -35/60
//        // degrees (-35 arc minutes), and if you want to compute the rise/set times
//        // for the Sun's upper limb, set h0 = -50/60 (-50 arc minutes).
//        //
//        double h0 = -50/60 * DEG_RAD;
//
//        double HA0 = ::acos(
//          (sin(h0) - ::sin(fLatitude) * sin_sDec) /
//          (cos(fLatitude) * cos(sDec*DEG_RAD)))*RAD_DEG;
//
//        // When HA0 has been computed, leave it as it is for the Sun but multiply
//        // by 365.2422/366.2422 for stellar objects, to convert from sidereal to
//        // solar time.  Finally compute:
//        //
//        //    Rise time  =  MT - HA0
//        //    Set  time  =  MT + HA0
//        //
//        // convert the times from degrees to hours by dividing by 15.
//        //
//        // If you'd like to check that your calculations are accurate or just
//        // need a quick result, check the USNO's Sun or Moon Rise/Set Table,
//        // <URL:http://aa.usno.navy.mil/AA/data/docs/RS_OneYear.html>.
//
//        double result = MT + (rise ? -HA0 : HA0); // in degrees
//
//        // Find UT midnight on this day
//        long midnight = DAY_MS * (time / DAY_MS);
//
//        return midnight + (long) (result * 3600000 / 15);
//    }

//-------------------------------------------------------------------------
// The Moon
//-------------------------------------------------------------------------

#define moonL0  (318.351648 * CalendarAstronomer::PI/180 )   // Mean long. at epoch
#define moonP0 ( 36.340410 * CalendarAstronomer::PI/180 )   // Mean long. of perigee
#define moonN0 ( 318.510107 * CalendarAstronomer::PI/180 )   // Mean long. of node
#define moonI  (   5.145366 * CalendarAstronomer::PI/180 )   // Inclination of orbit
#define moonE  (   0.054900 )            // Eccentricity of orbit

// These aren't used right now
#define moonA  (   3.84401e5 )           // semi-major axis (km)
#define moonT0 (   0.5181 * CalendarAstronomer::PI/180 )     // Angular size at distance A
#define moonPi (   0.9507 * CalendarAstronomer::PI/180 )     // Parallax at distance A

/**
 * The position of the moon at the time set on this
 * object, in equatorial coordinates.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
const CalendarAstronomer::Equatorial& CalendarAstronomer::getMoonPosition()
{
    //
    // See page 142 of "Practial Astronomy with your Calculator",
    // by Peter Duffet-Smith, for details on the algorithm.
    //
    if (moonPositionSet == FALSE) {
        // Calculate the solar longitude.  Has the side effect of
        // filling in "meanAnomalySun" as well.
        getSunLongitude();

        //
        // Find the # of days since the epoch of our orbital parameters.
        // TODO: Convert the time of day portion into ephemeris time
        //
        double day = getJulianDay() - JD_EPOCH;       // Days since epoch

        // Calculate the mean longitude and anomaly of the moon, based on
        // a circular orbit.  Similar to the corresponding solar calculation.
        double meanLongitude = norm2PI(13.1763966*PI/180*day + moonL0);
        meanAnomalyMoon = norm2PI(meanLongitude - 0.1114041*PI/180 * day - moonP0);

        //
        // Calculate the following corrections:
        //  Evection:   the sun's gravity affects the moon's eccentricity
        //  Annual Eqn: variation in the effect due to earth-sun distance
        //  A3:         correction factor (for ???)
        //
        double evection = 1.2739*PI/180 * ::sin(2 * (meanLongitude - sunLongitude)
            - meanAnomalyMoon);
        double annual   = 0.1858*PI/180 * ::sin(meanAnomalySun);
        double a3       = 0.3700*PI/180 * ::sin(meanAnomalySun);

        meanAnomalyMoon += evection - annual - a3;

        //
        // More correction factors:
        //  center  equation of the center correction
        //  a4      yet another error correction (???)
        //
        // TODO: Skip the equation of the center correction and solve Kepler's eqn?
        //
        double center = 6.2886*PI/180 * ::sin(meanAnomalyMoon);
        double a4 =     0.2140*PI/180 * ::sin(2 * meanAnomalyMoon);

        // Now find the moon's corrected longitude
        moonLongitude = meanLongitude + evection + center - annual + a4;

        //
        // And finally, find the variation, caused by the fact that the sun's
        // gravitational pull on the moon varies depending on which side of
        // the earth the moon is on
        //
        double variation = 0.6583*CalendarAstronomer::PI/180 * ::sin(2*(moonLongitude - sunLongitude));

        moonLongitude += variation;

        //
        // What we've calculated so far is the moon's longitude in the plane
        // of its own orbit.  Now map to the ecliptic to get the latitude
        // and longitude.  First we need to find the longitude of the ascending
        // node, the position on the ecliptic where it is crossed by the moon's
        // orbit as it crosses from the southern to the northern hemisphere.
        //
        double nodeLongitude = norm2PI(moonN0 - 0.0529539*PI/180 * day);

        nodeLongitude -= 0.16*PI/180 * ::sin(meanAnomalySun);

        double y = ::sin(moonLongitude - nodeLongitude);
        double x = cos(moonLongitude - nodeLongitude);

        moonEclipLong = ::atan2(y*cos(moonI), x) + nodeLongitude;
        double moonEclipLat = ::asin(y * ::sin(moonI));

        eclipticToEquatorial(moonPosition, moonEclipLong, moonEclipLat);
        moonPositionSet = TRUE;
    }
    return moonPosition;
}

/**
 * The "age" of the moon at the time specified in this object.
 * This is really the angle between the
 * current ecliptic longitudes of the sun and the moon,
 * measured in radians.
 *
 * @see #getMoonPhase
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::getMoonAge() {
    // See page 147 of "Practial Astronomy with your Calculator",
    // by Peter Duffet-Smith, for details on the algorithm.
    //
    // Force the moon's position to be calculated.  We're going to use
    // some the intermediate results cached during that calculation.
    //
    getMoonPosition();

    return norm2PI(moonEclipLong - sunLongitude);
}

/**
 * Calculate the phase of the moon at the time set in this object.
 * The returned phase is a <code>double</code> in the range
 * <code>0 <= phase < 1</code>, interpreted as follows:
 * <ul>
 * <li>0.00: New moon
 * <li>0.25: First quarter
 * <li>0.50: Full moon
 * <li>0.75: Last quarter
 * </ul>
 *
 * @see #getMoonAge
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
double CalendarAstronomer::getMoonPhase() {
    // See page 147 of "Practial Astronomy with your Calculator",
    // by Peter Duffet-Smith, for details on the algorithm.
    return 0.5 * (1 - cos(getMoonAge()));
}

/**
 * Constant representing a new moon.
 * For use with {@link #getMoonTime getMoonTime}
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
const CalendarAstronomer::MoonAge CalendarAstronomer::NEW_MOON() {
    return  CalendarAstronomer::MoonAge(0);
}

/**
 * Constant representing the moon's first quarter.
 * For use with {@link #getMoonTime getMoonTime}
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
/*const CalendarAstronomer::MoonAge CalendarAstronomer::FIRST_QUARTER() {
  return   CalendarAstronomer::MoonAge(CalendarAstronomer::PI/2);
}*/

/**
 * Constant representing a full moon.
 * For use with {@link #getMoonTime getMoonTime}
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
const CalendarAstronomer::MoonAge CalendarAstronomer::FULL_MOON() {
    return   CalendarAstronomer::MoonAge(CalendarAstronomer::PI);
}
/**
 * Constant representing the moon's last quarter.
 * For use with {@link #getMoonTime getMoonTime}
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */

class MoonTimeAngleFunc : public CalendarAstronomer::AngleFunc {
public:
    virtual ~MoonTimeAngleFunc();
    virtual double eval(CalendarAstronomer&a) { return a.getMoonAge(); }
};

MoonTimeAngleFunc::~MoonTimeAngleFunc() {}

/*const CalendarAstronomer::MoonAge CalendarAstronomer::LAST_QUARTER() {
  return  CalendarAstronomer::MoonAge((CalendarAstronomer::PI*3)/2);
}*/

/**
 * Find the next or previous time at which the Moon's ecliptic
 * longitude will have the desired value.
 * <p>
 * @param desired   The desired longitude.
 * @param next      <tt>true</tt> if the next occurrance of the phase
 *                  is desired, <tt>false</tt> for the previous occurrance.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
UDate CalendarAstronomer::getMoonTime(double desired, UBool next)
{
    MoonTimeAngleFunc func;
    return timeOfAngle( func,
                        desired,
                        SYNODIC_MONTH,
                        MINUTE_MS,
                        next);
}

/**
 * Find the next or previous time at which the moon will be in the
 * desired phase.
 * <p>
 * @param desired   The desired phase of the moon.
 * @param next      <tt>true</tt> if the next occurrance of the phase
 *                  is desired, <tt>false</tt> for the previous occurrance.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
UDate CalendarAstronomer::getMoonTime(const CalendarAstronomer::MoonAge& desired, UBool next) {
    return getMoonTime(desired.value, next);
}

class MoonRiseSetCoordFunc : public CalendarAstronomer::CoordFunc {
public:
    virtual ~MoonRiseSetCoordFunc();
    virtual void eval(CalendarAstronomer::Equatorial& result, CalendarAstronomer&a) { result = a.getMoonPosition(); }
};

MoonRiseSetCoordFunc::~MoonRiseSetCoordFunc() {}

/**
 * Returns the time (GMT) of sunrise or sunset on the local date to which
 * this calendar is currently set.
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
UDate CalendarAstronomer::getMoonRiseSet(UBool rise)
{
    MoonRiseSetCoordFunc func;
    return riseOrSet(func,
                     rise,
                     .533 * DEG_RAD,        // Angular Diameter
                     34 /60.0 * DEG_RAD,    // Refraction correction
                     MINUTE_MS);            // Desired accuracy
}

//-------------------------------------------------------------------------
// Interpolation methods for finding the time at which a given event occurs
//-------------------------------------------------------------------------

UDate CalendarAstronomer::timeOfAngle(AngleFunc& func, double desired,
                                      double periodDays, double epsilon, UBool next)
{
    // Find the value of the function at the current time
    double lastAngle = func.eval(*this);

    // Find out how far we are from the desired angle
    double deltaAngle = norm2PI(desired - lastAngle) ;

    // Using the average period, estimate the next (or previous) time at
    // which the desired angle occurs.
    double deltaT =  (deltaAngle + (next ? 0.0 : - CalendarAstronomer_PI2 )) * (periodDays*DAY_MS) / CalendarAstronomer_PI2;

    double lastDeltaT = deltaT; // Liu
    UDate startTime = fTime; // Liu

    setTime(fTime + uprv_ceil(deltaT));

    // Now iterate until we get the error below epsilon.  Throughout
    // this loop we use normPI to get values in the range -Pi to Pi,
    // since we're using them as correction factors rather than absolute angles.
    do {
        // Evaluate the function at the time we've estimated
        double angle = func.eval(*this);

        // Find the # of milliseconds per radian at this point on the curve
        double factor = uprv_fabs(deltaT / normPI(angle-lastAngle));

        // Correct the time estimate based on how far off the angle is
        deltaT = normPI(desired - angle) * factor;

        // HACK:
        //
        // If abs(deltaT) begins to diverge we need to quit this loop.
        // This only appears to happen when attempting to locate, for
        // example, a new moon on the day of the new moon.  E.g.:
        //
        // This result is correct:
        // newMoon(7508(Mon Jul 23 00:00:00 CST 1990,false))=
        //   Sun Jul 22 10:57:41 CST 1990
        //
        // But attempting to make the same call a day earlier causes deltaT
        // to diverge:
        // CalendarAstronomer.timeOfAngle() diverging: 1.348508727575625E9 ->
        //   1.3649828540224032E9
        // newMoon(7507(Sun Jul 22 00:00:00 CST 1990,false))=
        //   Sun Jul 08 13:56:15 CST 1990
        //
        // As a temporary solution, we catch this specific condition and
        // adjust our start time by one eighth period days (either forward
        // or backward) and try again.
        // Liu 11/9/00
        if (uprv_fabs(deltaT) > uprv_fabs(lastDeltaT)) {
            double delta = uprv_ceil (periodDays * DAY_MS / 8.0);
            setTime(startTime + (next ? delta : -delta));
            return timeOfAngle(func, desired, periodDays, epsilon, next);
        }

        lastDeltaT = deltaT;
        lastAngle = angle;

        setTime(fTime + uprv_ceil(deltaT));
    }
    while (uprv_fabs(deltaT) > epsilon);

    return fTime;
}

UDate CalendarAstronomer::riseOrSet(CoordFunc& func, UBool rise,
                                    double diameter, double refraction,
                                    double epsilon)
{
    Equatorial pos;
    double      tanL   = ::tan(fLatitude);
    double     deltaT = 0;
    int32_t         count = 0;

    //
    // Calculate the object's position at the current time, then use that
    // position to calculate the time of rising or setting.  The position
    // will be different at that time, so iterate until the error is allowable.
    //
    U_DEBUG_ASTRO_MSG(("setup rise=%s, dia=%.3lf, ref=%.3lf, eps=%.3lf\n",
        rise?"T":"F", diameter, refraction, epsilon));
    do {
        // See "Practical Astronomy With Your Calculator, section 33.
        func.eval(pos, *this);
        double angle = ::acos(-tanL * ::tan(pos.declination));
        double lst = ((rise ? CalendarAstronomer_PI2-angle : angle) + pos.ascension ) * 24 / CalendarAstronomer_PI2;

        // Convert from LST to Universal Time.
        UDate newTime = lstToUT( lst );

        deltaT = newTime - fTime;
        setTime(newTime);
        U_DEBUG_ASTRO_MSG(("%d] dT=%.3lf, angle=%.3lf, lst=%.3lf,   A=%.3lf/D=%.3lf\n",
            count, deltaT, angle, lst, pos.ascension, pos.declination));
    }
    while (++ count < 5 && uprv_fabs(deltaT) > epsilon);

    // Calculate the correction due to refraction and the object's angular diameter
    double cosD  = ::cos(pos.declination);
    double psi   = ::acos(sin(fLatitude) / cosD);
    double x     = diameter / 2 + refraction;
    double y     = ::asin(sin(x) / ::sin(psi));
    long  delta  = (long)((240 * y * RAD_DEG / cosD)*SECOND_MS);

    return fTime + (rise ? -delta : delta);
}
											   /**
 * Return the obliquity of the ecliptic (the angle between the ecliptic
 * and the earth's equator) at the current time.  This varies due to
 * the precession of the earth's axis.
 *
 * @return  the obliquity of the ecliptic relative to the equator,
 *          measured in radians.
 */
double CalendarAstronomer::eclipticObliquity() {
    if (isINVALID(eclipObliquity)) {
        const double epoch = 2451545.0;     // 2000 AD, January 1.5

        double T = (getJulianDay() - epoch) / 36525;

        eclipObliquity = 23.439292
            - 46.815/3600 * T
            - 0.0006/3600 * T*T
            + 0.00181/3600 * T*T*T;

        eclipObliquity *= DEG_RAD;
    }
    return eclipObliquity;
}


//-------------------------------------------------------------------------
// Private data
//-------------------------------------------------------------------------
void CalendarAstronomer::clearCache() {
    const double INVALID = uprv_getNaN();

    julianDay       = INVALID;
    julianCentury   = INVALID;
    sunLongitude    = INVALID;
    meanAnomalySun  = INVALID;
    moonLongitude   = INVALID;
    moonEclipLong   = INVALID;
    meanAnomalyMoon = INVALID;
    eclipObliquity  = INVALID;
    siderealTime    = INVALID;
    siderealT0      = INVALID;
    moonPositionSet = FALSE;
}

//private static void out(String s) {
//    System.out.println(s);
//}

//private static String deg(double rad) {
//    return Double.toString(rad * RAD_DEG);
//}

//private static String hours(long ms) {
//    return Double.toString((double)ms / HOUR_MS) + " hours";
//}

/**
 * @internal
 * @deprecated ICU 2.4. This class may be removed or modified.
 */
/*UDate CalendarAstronomer::local(UDate localMillis) {
  // TODO - srl ?
  TimeZone *tz = TimeZone::createDefault();
  int32_t rawOffset;
  int32_t dstOffset;
  UErrorCode status = U_ZERO_ERROR;
  tz->getOffset(localMillis, TRUE, rawOffset, dstOffset, status);
  delete tz;
  return localMillis - rawOffset;
}*/

// Debugging functions
UnicodeString CalendarAstronomer::Ecliptic::toString() const
{
#ifdef U_DEBUG_ASTRO
    char tmp[800];
    sprintf(tmp, "[%.5f,%.5f]", longitude*RAD_DEG, latitude*RAD_DEG);
    return UnicodeString(tmp, "");
#else
    return UnicodeString();
#endif
}

UnicodeString CalendarAstronomer::Equatorial::toString() const
{
#ifdef U_DEBUG_ASTRO
    char tmp[400];
    sprintf(tmp, "%f,%f",
        (ascension*RAD_DEG), (declination*RAD_DEG));
    return UnicodeString(tmp, "");
#else
    return UnicodeString();
#endif
}

UnicodeString CalendarAstronomer::Horizon::toString() const
{
#ifdef U_DEBUG_ASTRO
    char tmp[800];
    sprintf(tmp, "[%.5f,%.5f]", altitude*RAD_DEG, azimuth*RAD_DEG);
    return UnicodeString(tmp, "");
#else
    return UnicodeString();
#endif
}


//  static private String radToHms(double angle) {
//    int hrs = (int) (angle*RAD_HOUR);
//    int min = (int)((angle*RAD_HOUR - hrs) * 60);
//    int sec = (int)((angle*RAD_HOUR - hrs - min/60.0) * 3600);

//    return Integer.toString(hrs) + "h" + min + "m" + sec + "s";
//  }

//  static private String radToDms(double angle) {
//    int deg = (int) (angle*RAD_DEG);
//    int min = (int)((angle*RAD_DEG - deg) * 60);
//    int sec = (int)((angle*RAD_DEG - deg - min/60.0) * 3600);

//    return Integer.toString(deg) + "\u00b0" + min + "'" + sec + "\"";
//  }

// =============== Calendar Cache ================

void CalendarCache::createCache(CalendarCache** cache, UErrorCode& status) {
    ucln_i18n_registerCleanup(UCLN_I18N_ASTRO_CALENDAR, calendar_astro_cleanup);
    if(cache == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    } else {
        *cache = new CalendarCache(32, status);
        if(U_FAILURE(status)) {
            delete *cache;
            *cache = NULL;
        }
    }
}

int32_t CalendarCache::get(CalendarCache** cache, int32_t key, UErrorCode &status) {
    int32_t res;

    if(U_FAILURE(status)) {
        return 0;
    }
    umtx_lock(&ccLock);

    if(*cache == NULL) {
        createCache(cache, status);
        if(U_FAILURE(status)) {
            umtx_unlock(&ccLock);
            return 0;
        }
    }

    res = uhash_igeti((*cache)->fTable, key);
    U_DEBUG_ASTRO_MSG(("%p: GET: [%d] == %d\n", (*cache)->fTable, key, res));

    umtx_unlock(&ccLock);
    return res;
}

void CalendarCache::put(CalendarCache** cache, int32_t key, int32_t value, UErrorCode &status) {
    if(U_FAILURE(status)) {
        return;
    }
    umtx_lock(&ccLock);

    if(*cache == NULL) {
        createCache(cache, status);
        if(U_FAILURE(status)) {
            umtx_unlock(&ccLock);
            return;
        }
    }

    uhash_iputi((*cache)->fTable, key, value, &status);
    U_DEBUG_ASTRO_MSG(("%p: PUT: [%d] := %d\n", (*cache)->fTable, key, value));

    umtx_unlock(&ccLock);
}

CalendarCache::CalendarCache(int32_t size, UErrorCode &status) {
    fTable = uhash_openSize(uhash_hashLong, uhash_compareLong, NULL, size, &status);
    U_DEBUG_ASTRO_MSG(("%p: Opening.\n", fTable));
}

CalendarCache::~CalendarCache() {
    if(fTable != NULL) {
        U_DEBUG_ASTRO_MSG(("%p: Closing.\n", fTable));
        uhash_close(fTable);
    }
}

U_NAMESPACE_END

#endif //  !UCONFIG_NO_FORMATTING
