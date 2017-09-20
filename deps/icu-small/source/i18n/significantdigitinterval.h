// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* significantdigitinterval.h
*
* created on: 2015jan6
* created by: Travis Keep
*/

#ifndef __SIGNIFICANTDIGITINTERVAL_H__
#define __SIGNIFICANTDIGITINTERVAL_H__

#include "unicode/uobject.h"
#include "unicode/utypes.h"

U_NAMESPACE_BEGIN

/**
 * An interval of allowed significant digit counts.
 */
class U_I18N_API SignificantDigitInterval : public UMemory {
public:

    /**
     * No limits on significant digits.
     */
    SignificantDigitInterval()
            : fMax(INT32_MAX), fMin(0) { }

    /**
     * Make this instance have no limit on significant digits.
     */
    void clear() {
        fMin = 0;
        fMax = INT32_MAX;
    }

    /**
     * Returns TRUE if this object is equal to rhs.
     */
    UBool equals(const SignificantDigitInterval &rhs) const {
        return ((fMax == rhs.fMax) && (fMin == rhs.fMin));
    }

    /**
     * Sets maximum significant digits. 0 or negative means no maximum.
     */
    void setMax(int32_t count) {
        fMax = count <= 0 ? INT32_MAX : count;
    }

    /**
     * Get maximum significant digits. INT32_MAX means no maximum.
     */
    int32_t getMax() const {
        return fMax;
    }

    /**
     * Sets minimum significant digits. 0 or negative means no minimum.
     */
    void setMin(int32_t count) {
        fMin = count <= 0 ? 0 : count;
    }

    /**
     * Get maximum significant digits. 0 means no minimum.
     */
    int32_t getMin() const {
        return fMin;
    }

    /**
     * Returns TRUE if this instance represents no constraints on significant
     * digits.
     */
    UBool isNoConstraints() const {
        return fMin == 0 && fMax == INT32_MAX;
    }

private:
    int32_t fMax;
    int32_t fMin;
};

U_NAMESPACE_END

#endif  // __SIGNIFICANTDIGITINTERVAL_H__
