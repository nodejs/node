// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2009-2017, International Business Machines Corporation,       *
 * Google, and others. All Rights Reserved.                                    *
 *******************************************************************************
 */

#ifndef __NOUNIT_H__
#define __NOUNIT_H__

#include "unicode/utypes.h"

#if U_SHOW_CPLUSPLUS_API

#if !UCONFIG_NO_FORMATTING

#include "unicode/measunit.h"

#ifndef U_HIDE_DRAFT_API

/**
 * \file
 * \brief C++ API: units for percent and permille
 */

U_NAMESPACE_BEGIN

/**
 * Dimensionless unit for percent and permille.
 * @see NumberFormatter
 * @draft ICU 60
 */
class U_I18N_API NoUnit: public MeasureUnit {
public:
    /**
     * Returns an instance for the base unit (dimensionless and no scaling).
     *
     * @return               a NoUnit instance
     * @draft ICU 60
     */
    static NoUnit U_EXPORT2 base();

    /**
     * Returns an instance for percent, or 1/100 of a base unit.
     *
     * @return               a NoUnit instance
     * @draft ICU 60
     */
    static NoUnit U_EXPORT2 percent();

    /**
     * Returns an instance for permille, or 1/1000 of a base unit.
     *
     * @return               a NoUnit instance
     * @draft ICU 60
     */
    static NoUnit U_EXPORT2 permille();

    /**
     * Copy operator.
     * @draft ICU 60
     */
    NoUnit(const NoUnit& other);

    /**
     * Destructor.
     * @draft ICU 60
     */
    virtual ~NoUnit();

    /**
     * Return a polymorphic clone of this object.  The result will
     * have the same class as returned by getDynamicClassID().
     * @draft ICU 60
     */
    virtual NoUnit* clone() const;

    /**
     * Returns a unique class ID for this object POLYMORPHICALLY.
     * This method implements a simple form of RTTI used by ICU.
     * @return The class ID for this object. All objects of a given
     * class have the same class ID.  Objects of other classes have
     * different class IDs.
     * @draft ICU 60
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * Returns the class ID for this class. This is used to compare to
     * the return value of getDynamicClassID().
     * @return The class ID for all objects of this class.
     * @draft ICU 60
     */
    static UClassID U_EXPORT2 getStaticClassID();

private:
    /**
     * Constructor
     * @internal (private)
     */
    NoUnit(const char* subtype);

};

U_NAMESPACE_END

#endif  /* U_HIDE_DRAFT_API */
#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // __NOUNIT_H__
//eof
//
