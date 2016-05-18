/*
*******************************************************************************
* Copyright (C) 2009-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/
#ifndef __ZTRANS_H
#define __ZTRANS_H

/**
 * \file
 * \brief C API: Time zone transition classes
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"

#ifndef UCNV_H

/**
 * A TimeZoneTransition.  Use the ztrans_* API to manipulate.  Create with
 * ztrans_open*, and destroy with ztrans_close.
 */
struct ZTrans;
typedef struct ZTrans ZTrans;

#endif

/**
 * Constructs a time zone transition with the time and the rules before/after
 * the transition.
 *
 * @param time  The time of transition in milliseconds since the base time.
 * @param from  The time zone rule used before the transition.
 * @param to    The time zone rule used after the transition.
 */
U_CAPI ZTrans* U_EXPORT2
ztrans_open(UDate time, const void* from, const void* to);

/**
 * Constructs an empty <code>TimeZoneTransition</code>
 */
U_CAPI ZTrans* U_EXPORT2
ztrans_openEmpty();

/**
 * Disposes of the storage used by a ZTrans object.  This function should
 * be called exactly once for objects returned by ztrans_open*.
 * @param trans the object to dispose of
 */
U_CAPI void U_EXPORT2
ztrans_close(ZTrans *trans);

/**
 * Returns a copy of this object.
 * @param rule the original ZRule
 * @return the newly allocated copy of the ZRule
 */
U_CAPI ZTrans* U_EXPORT2
ztrans_clone(ZTrans *trans);

/**
 * Returns true if trans1 is identical to trans2
 * and vis versa.
 * @param trans1 to be checked for containment
 * @param trans2 to be checked for containment
 * @return true if the test condition is met
 */
U_CAPI UBool U_EXPORT2
ztrans_equals(const ZTrans* trans1, const ZTrans* trans2);

/**
 * Returns the time of transition in milliseconds.
 * param trans, the transition to use
 * @return The time of the transition in milliseconds since the 1970 Jan 1 epoch time.
 */
U_CAPI UDate U_EXPORT2
ztrans_getTime(ZTrans* trans);

/**
 * Sets the time of transition in milliseconds.
 * param trans, the transition to use
 * @param time The time of the transition in milliseconds since the 1970 Jan 1 epoch time.
 */
U_CAPI void U_EXPORT2
ztrans_setTime(ZTrans* trans, UDate time);

/**
 * Returns the rule used before the transition.
 * param trans, the transition to use
 * @return The time zone rule used after the transition.
 */
U_CAPI void* U_EXPORT2
ztrans_getFrom(ZTrans* & trans);

/**
 * Sets the rule used before the transition.  The caller remains
 * responsible for deleting the TimeZoneRule object.
 * param trans, the transition to use
 * param trans, the transition to use
 * @param from The time zone rule used before the transition.
 */
U_CAPI void U_EXPORT2
ztrans_setFrom(ZTrans* trans, const void* from);

/**
 * Adopts the rule used before the transition.  The caller must
 * not delete the TimeZoneRule object passed in.
 * param trans, the transition to use
 * @param from The time zone rule used before the transition.
 */
U_CAPI void U_EXPORT2
ztrans_adoptFrom(ZTrans* trans, void* from);

/**
 * Returns the rule used after the transition.
 * param trans, the transition to use
 * @return The time zone rule used after the transition.
 */
U_CAPI void* U_EXPORT2
ztrans_getTo(ZTrans* trans);

/**
 * Sets the rule used after the transition.  The caller remains
 * responsible for deleting the TimeZoneRule object.
 * param trans, the transition to use
 * @param to The time zone rule used after the transition.
 */
U_CAPI void U_EXPORT2
ztrans_setTo(ZTrans* trans, const void* to);

/**
 * Adopts the rule used after the transition.  The caller must
 * not delete the TimeZoneRule object passed in.
 * param trans, the transition to use
 * @param to The time zone rule used after the transition.
 */
U_CAPI void U_EXPORT2
ztrans_adoptTo(ZTrans* trans, void* to);

/**
 * Return the class ID for this class. This is useful only for comparing to
 * a return value from getDynamicClassID(). For example:
 * <pre>
 * .   Base* polymorphic_pointer = createPolymorphicObject();
 * .   if (polymorphic_pointer->getDynamicClassID() ==
 * .       erived::getStaticClassID()) ...
 * </pre>
 * param trans, the transition to use
 * @return          The class ID for all objects of this class.
 */
U_CAPI UClassID U_EXPORT2
ztrans_getStaticClassID(ZTrans* trans);

/**
 * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
 * method is to implement a simple version of RTTI, since not all C++
 * compilers support genuine RTTI. Polymorphic operator==() and clone()
 * methods call this method.
 *
 * param trans, the transition to use
 * @return          The class ID for this object. All objects of a
 *                  given class have the same class ID.  Objects of
 *                  other classes have different class IDs.
 */
U_CAPI UClassID U_EXPORT2
ztrans_getDynamicClassID(ZTrans* trans);

#endif

#endif
