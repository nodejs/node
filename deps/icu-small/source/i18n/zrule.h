/*
*******************************************************************************
* Copyright (C) 2009-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/
#ifndef __ZRULE_H
#define __ZRULE_H

/**
 * \file 
 * \brief C API: Time zone rule classes
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"

#ifndef UCNV_H

/**
 * A TimeZoneRule.  Use the zrule_* API to manipulate.  Create with
 * zrule_open*, and destroy with zrule_close.
 */
struct ZRule;
typedef struct ZRule ZRule;

/**
 * An InitialTimeZoneRule.  Use the izrule_* API to manipulate.  Create with
 * izrule_open*, and destroy with izrule_close.
 */
struct IZRule;
typedef struct IZRule IZRule;

/**
 * An AnnualTimeZoneRule.  Use the azrule_* API to manipulate.  Create with
 * azrule_open*, and destroy with azrule_close.
 */
struct AZRule;
typedef struct AZRule AZRule;

#endif

/*********************************************************************
 * ZRule API
 *********************************************************************/

/**
 * Disposes of the storage used by a ZRule object.  This function should
 * be called exactly once for objects returned by zrule_open*.
 * @param set the object to dispose of
 */
U_CAPI void U_EXPORT2
zrule_close(ZRule* rule);

/**
 * Returns true if rule1 is identical to rule2
 * and vis versa.
 * @param rule1 to be checked for containment
 * @param rule2 to be checked for containment
 * @return true if the test condition is met
 */
U_CAPI UBool U_EXPORT2
zrule_equals(const ZRule* rule1, const ZRule* rule2);

/**
 * Fills in "name" with the name of this time zone.
 * @param rule, the Zrule to use
 * @param name  Receives the name of this time zone.
 * @param nameLength, length of the returned name
 */
U_CAPI void U_EXPORT2
zrule_getName(ZRule* rule, UChar* name, int32_t nameLength);

/**
 * Gets the standard time offset.
 * @param rule, the Zrule to use
 * @return  The standard time offset from UTC in milliseconds.
 */
U_CAPI int32_t U_EXPORT2
zrule_getRawOffset(ZRule* rule);

/**
 * Gets the amount of daylight saving delta time from the standard time.
 * @param rule, the Zrule to use
 * @return  The amount of daylight saving offset used by this rule
 *          in milliseconds.
 */
U_CAPI int32_t U_EXPORT2
zrule_getDSTSavings(ZRule* rule);

/**
 * Returns if this rule represents the same rule and offsets as another.
 * When two ZRule objects differ only its names, this method
 * returns true.
 * @param rule1 to be checked for containment
 * @param rule2 to be checked for containment
 * @return  true if the other <code>TimeZoneRule</code> is the same as this one.
 */
U_CAPI UBool U_EXPORT2
zrule_isEquivalentTo(ZRule* rule1,  ZRule* rule2);

/*********************************************************************
 * IZRule API
 *********************************************************************/

/**
 * Constructs an IZRule with the name, the GMT offset of its
 * standard time and the amount of daylight saving offset adjustment.
 * @param name          The time zone name.
 * @param nameLength    The length of the time zone name.
 * @param rawOffset     The UTC offset of its standard time in milliseconds.
 * @param dstSavings    The amount of daylight saving offset adjustment in milliseconds.
 *                      If this ia a rule for standard time, the value of this argument is 0.
 */
U_CAPI IZRule* U_EXPORT2
izrule_open(const UChar* name, int32_t nameLength, int32_t rawOffset, int32_t dstSavings);

/**
 * Disposes of the storage used by a IZRule object.  This function should
 * be called exactly once for objects returned by izrule_open*.
 * @param set the object to dispose of
 */
U_CAPI void U_EXPORT2
izrule_close(IZRule* rule);

/**
 * Returns a copy of this object.
 * @param rule the original IZRule
 * @return the newly allocated copy of the IZRule
 */
U_CAPI IZRule* U_EXPORT2
izrule_clone(IZRule *rule);

/**
 * Returns true if rule1 is identical to rule2
 * and vis versa.
 * @param rule1 to be checked for containment
 * @param rule2 to be checked for containment
 * @return true if the test condition is met
 */
U_CAPI UBool U_EXPORT2
izrule_equals(const IZRule* rule1, const IZRule* rule2);

/**
 * Fills in "name" with the name of this time zone.
 * @param rule, the IZrule to use
 * @param name  Receives the name of this time zone.
 * @param nameLength, length of the returned name
 */
U_CAPI void U_EXPORT2
izrule_getName(IZRule* rule, UChar* & name, int32_t & nameLength);

/**
 * Gets the standard time offset.
 * @param rule, the IZrule to use
 * @return  The standard time offset from UTC in milliseconds.
 */
U_CAPI int32_t U_EXPORT2
izrule_getRawOffset(IZRule* rule);

/**
 * Gets the amount of daylight saving delta time from the standard time.
 * @param rule, the IZrule to use
 * @return  The amount of daylight saving offset used by this rule
 *          in milliseconds.
 */
U_CAPI int32_t U_EXPORT2
izrule_getDSTSavings(IZRule* rule);

/**
 * Returns if this rule represents the same rule and offsets as another.
 * When two IZRule objects differ only its names, this method
 * returns true.
 * @param rule1 to be checked for containment
 * @param rule2 to be checked for containment
 * @return  true if the other <code>TimeZoneRule</code> is the same as this one.
 */
U_CAPI UBool U_EXPORT2
izrule_isEquivalentTo(IZRule* rule1,  IZRule* rule2);

/**
 * Gets the very first time when this rule takes effect.
 * @param rule              The IZrule to use
 * @param prevRawOffset     The standard time offset from UTC before this rule
 *                          takes effect in milliseconds.
 * @param prevDSTSavings    The amount of daylight saving offset from the
 *                          standard time.
 * @param result            Receives the very first time when this rule takes effect.
 * @return  true if the start time is available.  When false is returned, output parameter
 *          "result" is unchanged.
 */
U_CAPI UBool U_EXPORT2
izrule_getFirstStart(IZRule* rule, int32_t prevRawOffset, int32_t prevDSTSavings, 
                    UDate& result);

/**
 * Gets the final time when this rule takes effect.
 * @param rule              The IZrule to use     
 * @param prevRawOffset     The standard time offset from UTC before this rule
 *                          takes effect in milliseconds.
 * @param prevDSTSavings    The amount of daylight saving offset from the
 *                          standard time.
 * @param result            Receives the final time when this rule takes effect.
 * @return  true if the start time is available.  When false is returned, output parameter
 *          "result" is unchanged.
 */
U_CAPI UBool U_EXPORT2
izrule_getFinalStart(IZRule* rule, int32_t prevRawOffset, int32_t prevDSTSavings, 
                    UDate& result);

/**
 * Gets the first time when this rule takes effect after the specified time.
 * @param rule              The IZrule to use
 * @param base              The first start time after this base time will be returned.
 * @param prevRawOffset     The standard time offset from UTC before this rule
 *                          takes effect in milliseconds.
 * @param prevDSTSavings    The amount of daylight saving offset from the
 *                          standard time.
 * @param inclusive         Whether the base time is inclusive or not.
 * @param result            Receives The first time when this rule takes effect after
 *                          the specified base time.
 * @return  true if the start time is available.  When false is returned, output parameter
 *          "result" is unchanged.
 */
U_CAPI UBool U_EXPORT2
izrule_getNextStart(IZRule* rule, UDate base, int32_t prevRawOffset, 
                   int32_t prevDSTSavings, UBool inclusive, UDate& result);

/**
 * Gets the most recent time when this rule takes effect before the specified time.
 * @param rule              The IZrule to use
 * @param base              The most recent time before this base time will be returned.
 * @param prevRawOffset     The standard time offset from UTC before this rule
 *                          takes effect in milliseconds.
 * @param prevDSTSavings    The amount of daylight saving offset from the
 *                          standard time.
 * @param inclusive         Whether the base time is inclusive or not.
 * @param result            Receives The most recent time when this rule takes effect before
 *                          the specified base time.
 * @return  true if the start time is available.  When false is returned, output parameter
 *          "result" is unchanged.
 */
U_CAPI UBool U_EXPORT2
izrule_getPreviousStart(IZRule* rule, UDate base, int32_t prevRawOffset, 
                       int32_t prevDSTSavings, UBool inclusive, UDate& result);


/**
 * Return the class ID for this class. This is useful only for comparing to
 * a return value from getDynamicClassID(). For example:
 * <pre>
 * .   Base* polymorphic_pointer = createPolymorphicObject();
 * .   if (polymorphic_pointer->getDynamicClassID() ==
 * .       erived::getStaticClassID()) ...
 * </pre>
 * @param rule              The IZrule to use
 * @return          The class ID for all objects of this class.
 */
U_CAPI UClassID U_EXPORT2
izrule_getStaticClassID(IZRule* rule);

/**
 * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
 * method is to implement a simple version of RTTI, since not all C++
 * compilers support genuine RTTI. Polymorphic operator==() and clone()
 * methods call this method.
 *
 * @param rule              The IZrule to use
 * @return          The class ID for this object. All objects of a
 *                  given class have the same class ID.  Objects of
 *                  other classes have different class IDs.
 */
U_CAPI UClassID U_EXPORT2
izrule_getDynamicClassID(IZRule* rule);

#endif

#endif
