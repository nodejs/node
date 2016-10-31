// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1997-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File DIGITLST.CPP
*
* Modification History:
*
*   Date        Name        Description
*   03/21/97    clhuang     Converted from java.
*   03/21/97    clhuang     Implemented with new APIs.
*   03/27/97    helena      Updated to pass the simple test after code review.
*   03/31/97    aliu        Moved isLONG_MIN to here, and fixed it.
*   04/15/97    aliu        Changed MAX_COUNT to DBL_DIG.  Changed Digit to char.
*                           Reworked representation by replacing fDecimalAt
*                           with fExponent.
*   04/16/97    aliu        Rewrote set() and getDouble() to use sprintf/atof
*                           to do digit conversion.
*   09/09/97    aliu        Modified for exponential notation support.
*   08/02/98    stephen     Added nearest/even rounding
*                            Fixed bug in fitsIntoLong
******************************************************************************
*/

#if defined(__CYGWIN__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "digitlst.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/putil.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "mutex.h"
#include "putilimp.h"
#include "uassert.h"
#include "digitinterval.h" 
#include "ucln_in.h"
#include "umutex.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <limits>

#if !defined(U_USE_STRTOD_L)
# if U_PLATFORM_USES_ONLY_WIN32_API
#   define U_USE_STRTOD_L 1
# elif defined(U_HAVE_STRTOD_L)
#   define U_USE_STRTOD_L U_HAVE_STRTOD_L
# else
#   define U_USE_STRTOD_L 0
# endif
#endif

#if U_USE_STRTOD_L && !U_PLATFORM_USES_ONLY_WIN32_API
# if U_PLATFORM == U_PF_CYGWIN
#   include <locale.h>
# else
#   include <xlocale.h>
# endif
#endif

// ***************************************************************************
// class DigitList
//    A wrapper onto decNumber.
//    Used to be standalone.
// ***************************************************************************

/**
 * This is the zero digit.  The base for the digits returned by getDigit()
 * Note that it is the platform invariant digit, and is not Unicode.
 */
#define kZero '0'


/* Only for 32 bit numbers. Ignore the negative sign. */
//static const char LONG_MIN_REP[] = "2147483648";
//static const char I64_MIN_REP[] = "9223372036854775808";


U_NAMESPACE_BEGIN

// -------------------------------------
// default constructor

DigitList::DigitList()
{
    uprv_decContextDefault(&fContext, DEC_INIT_BASE);
    fContext.traps  = 0;
    uprv_decContextSetRounding(&fContext, DEC_ROUND_HALF_EVEN);
    fContext.digits = fStorage.getCapacity();

    fDecNumber = fStorage.getAlias();
    uprv_decNumberZero(fDecNumber);

    internalSetDouble(0.0);
}

// -------------------------------------

DigitList::~DigitList()
{
}

// -------------------------------------
// copy constructor

DigitList::DigitList(const DigitList &other)
{
    fDecNumber = fStorage.getAlias();
    *this = other;
}


// -------------------------------------
// assignment operator

DigitList&
DigitList::operator=(const DigitList& other)
{
    if (this != &other)
    {
        uprv_memcpy(&fContext, &other.fContext, sizeof(decContext));

        if (other.fStorage.getCapacity() > fStorage.getCapacity()) {
            fDecNumber = fStorage.resize(other.fStorage.getCapacity());
        }
        // Always reset the fContext.digits, even if fDecNumber was not reallocated,
        // because above we copied fContext from other.fContext.
        fContext.digits = fStorage.getCapacity();
        uprv_decNumberCopy(fDecNumber, other.fDecNumber);

        {
            // fDouble is lazily created and cached.
            // Avoid potential races with that happening with other.fDouble
            // while we are doing the assignment.
            Mutex mutex;

            if(other.fHave==kDouble) {
                fUnion.fDouble = other.fUnion.fDouble;
            }
            fHave = other.fHave;
        }
    }
    return *this;
}

// -------------------------------------
//    operator ==  (does not exactly match the old DigitList function)

UBool
DigitList::operator==(const DigitList& that) const
{
    if (this == &that) {
        return TRUE;
    }
    decNumber n;  // Has space for only a none digit value.
    decContext c;
    uprv_decContextDefault(&c, DEC_INIT_BASE);
    c.digits = 1;
    c.traps = 0;

    uprv_decNumberCompare(&n, this->fDecNumber, that.fDecNumber, &c);
    UBool result = decNumberIsZero(&n);
    return result;
}

// -------------------------------------
//      comparison function.   Returns 
//         Not Comparable :  -2
//                      < :  -1
//                     == :   0
//                      > :  +1
int32_t DigitList::compare(const DigitList &other) {
    decNumber   result;
    int32_t     savedDigits = fContext.digits;
    fContext.digits = 1;
    uprv_decNumberCompare(&result, this->fDecNumber, other.fDecNumber, &fContext);
    fContext.digits = savedDigits;
    if (decNumberIsZero(&result)) {
        return 0;
    } else if (decNumberIsSpecial(&result)) {
        return -2;
    } else if (result.bits & DECNEG) {
        return -1;
    } else {
        return 1;
    }
}


// -------------------------------------
//  Reduce - remove trailing zero digits.
void
DigitList::reduce() {
    uprv_decNumberReduce(fDecNumber, fDecNumber, &fContext);
}


// -------------------------------------
//  trim - remove trailing fraction zero digits.
void
DigitList::trim() {
    uprv_decNumberTrim(fDecNumber);
}

// -------------------------------------
// Resets the digit list; sets all the digits to zero.

void
DigitList::clear()
{
    uprv_decNumberZero(fDecNumber);
    uprv_decContextSetRounding(&fContext, DEC_ROUND_HALF_EVEN);
    internalSetDouble(0.0);
}


/**
 * Formats a int64_t number into a base 10 string representation, and NULL terminates it.
 * @param number The number to format
 * @param outputStr The string to output to.  Must be at least MAX_DIGITS+2 in length (21),
 *                  to hold the longest int64_t value.
 * @return the number of digits written, not including the sign.
 */
static int32_t
formatBase10(int64_t number, char *outputStr) {
    // The number is output backwards, starting with the LSD.
    // Fill the buffer from the far end.  After the number is complete,
    // slide the string contents to the front.

    const int32_t MAX_IDX = MAX_DIGITS+2;
    int32_t destIdx = MAX_IDX;
    outputStr[--destIdx] = 0; 

    int64_t  n = number;
    if (number < 0) {   // Negative numbers are slightly larger than a postive
        outputStr[--destIdx] = (char)(-(n % 10) + kZero);
        n /= -10;
    }
    do { 
        outputStr[--destIdx] = (char)(n % 10 + kZero);
        n /= 10;
    } while (n > 0);
    
    if (number < 0) {
        outputStr[--destIdx] = '-';
    }

    // Slide the number to the start of the output str
    U_ASSERT(destIdx >= 0);
    int32_t length = MAX_IDX - destIdx;
    uprv_memmove(outputStr, outputStr+MAX_IDX-length, length);

    return length;
}


// -------------------------------------
//
//  setRoundingMode()
//    For most modes, the meaning and names are the same between the decNumber library
//      (which DigitList follows) and the ICU Formatting Rounding Mode values.
//      The flag constants are different, however.
//
//     Note that ICU's kRoundingUnnecessary is not implemented directly by DigitList.
//     This mode, inherited from Java, means that numbers that would not format exactly
//     will return an error when formatting is attempted.

void 
DigitList::setRoundingMode(DecimalFormat::ERoundingMode m) {
    enum rounding r;

    switch (m) {
      case  DecimalFormat::kRoundCeiling:  r = DEC_ROUND_CEILING;   break;
      case  DecimalFormat::kRoundFloor:    r = DEC_ROUND_FLOOR;     break;
      case  DecimalFormat::kRoundDown:     r = DEC_ROUND_DOWN;      break;
      case  DecimalFormat::kRoundUp:       r = DEC_ROUND_UP;        break;
      case  DecimalFormat::kRoundHalfEven: r = DEC_ROUND_HALF_EVEN; break;
      case  DecimalFormat::kRoundHalfDown: r = DEC_ROUND_HALF_DOWN; break;
      case  DecimalFormat::kRoundHalfUp:   r = DEC_ROUND_HALF_UP;   break;
      case  DecimalFormat::kRoundUnnecessary: r = DEC_ROUND_HALF_EVEN; break;
      default:
         // TODO: how to report the problem?
         // Leave existing mode unchanged.
         r = uprv_decContextGetRounding(&fContext);
    }
    uprv_decContextSetRounding(&fContext, r);
  
}


// -------------------------------------

void  
DigitList::setPositive(UBool s) {
    if (s) {
        fDecNumber->bits &= ~DECNEG; 
    } else {
        fDecNumber->bits |= DECNEG;
    }
    internalClear();
}
// -------------------------------------

void     
DigitList::setDecimalAt(int32_t d) {
    U_ASSERT((fDecNumber->bits & DECSPECIAL) == 0);  // Not Infinity or NaN
    U_ASSERT(d-1>-999999999);
    U_ASSERT(d-1< 999999999);
    int32_t adjustedDigits = fDecNumber->digits;
    if (decNumberIsZero(fDecNumber)) {
        // Account for difference in how zero is represented between DigitList & decNumber.
        adjustedDigits = 0;
    }
    fDecNumber->exponent = d - adjustedDigits;
    internalClear();
}

int32_t  
DigitList::getDecimalAt() {
    U_ASSERT((fDecNumber->bits & DECSPECIAL) == 0);  // Not Infinity or NaN
    if (decNumberIsZero(fDecNumber) || ((fDecNumber->bits & DECSPECIAL) != 0)) {
        return fDecNumber->exponent;  // Exponent should be zero for these cases.
    }
    return fDecNumber->exponent + fDecNumber->digits;
}

void     
DigitList::setCount(int32_t c)  {
    U_ASSERT(c <= fContext.digits);
    if (c == 0) {
        // For a value of zero, DigitList sets all fields to zero, while
        // decNumber keeps one digit (with that digit being a zero)
        c = 1;
        fDecNumber->lsu[0] = 0;
    }
    fDecNumber->digits = c;
    internalClear();
}

int32_t  
DigitList::getCount() const {
    if (decNumberIsZero(fDecNumber) && fDecNumber->exponent==0) {
       // The extra test for exponent==0 is needed because parsing sometimes appends
       // zero digits.  It's bogus, decimalFormatter parsing needs to be cleaned up.
       return 0;
    } else {
       return fDecNumber->digits;
    }
}
    
void     
DigitList::setDigit(int32_t i, char v) {
    int32_t count = fDecNumber->digits;
    U_ASSERT(i<count);
    U_ASSERT(v>='0' && v<='9');
    v &= 0x0f;
    fDecNumber->lsu[count-i-1] = v;
    internalClear();
}

char     
DigitList::getDigit(int32_t i) {
    int32_t count = fDecNumber->digits;
    U_ASSERT(i<count);
    return fDecNumber->lsu[count-i-1] + '0';
}

// copied from DigitList::getDigit()
uint8_t
DigitList::getDigitValue(int32_t i) {
    int32_t count = fDecNumber->digits;
    U_ASSERT(i<count);
    return fDecNumber->lsu[count-i-1];
}

// -------------------------------------
// Appends the digit to the digit list if it's not out of scope.
// Ignores the digit, otherwise.
// 
// This function is horribly inefficient to implement with decNumber because
// the digits are stored least significant first, which requires moving all
// existing digits down one to make space for the new one to be appended.
//
void
DigitList::append(char digit)
{
    U_ASSERT(digit>='0' && digit<='9');
    // Ignore digits which exceed the precision we can represent
    //    And don't fix for larger precision.  Fix callers instead.
    if (decNumberIsZero(fDecNumber)) {
        // Zero needs to be special cased because of the difference in the way
        // that the old DigitList and decNumber represent it.
        // digit cout was zero for digitList, is one for decNumber
        fDecNumber->lsu[0] = digit & 0x0f;
        fDecNumber->digits = 1;
        fDecNumber->exponent--;     // To match the old digit list implementation.
    } else {
        int32_t nDigits = fDecNumber->digits;
        if (nDigits < fContext.digits) {
            int i;
            for (i=nDigits; i>0; i--) {
                fDecNumber->lsu[i] = fDecNumber->lsu[i-1];
            }
            fDecNumber->lsu[0] = digit & 0x0f;
            fDecNumber->digits++;
            // DigitList emulation - appending doesn't change the magnitude of existing
            //                       digits.  With decNumber's decimal being after the
            //                       least signficant digit, we need to adjust the exponent.
            fDecNumber->exponent--;
        }
    }
    internalClear();
}

// -------------------------------------

/**
 * Currently, getDouble() depends on strtod() to do its conversion.
 *
 * WARNING!!
 * This is an extremely costly function. ~1/2 of the conversion time
 * can be linked to this function.
 */
double
DigitList::getDouble() const
{
    {
        Mutex mutex;
        if (fHave == kDouble) {
            return fUnion.fDouble;
        }
    }

    double tDouble = 0.0;
    if (isZero()) {
        tDouble = 0.0;
        if (decNumberIsNegative(fDecNumber)) {
            tDouble /= -1;
        }
    } else if (isInfinite()) {
        if (std::numeric_limits<double>::has_infinity) {
            tDouble = std::numeric_limits<double>::infinity();
        } else {
            tDouble = std::numeric_limits<double>::max();
        }
        if (!isPositive()) {
            tDouble = -tDouble; //this was incorrectly "-fDouble" originally.
        } 
    } else {
        MaybeStackArray<char, MAX_DBL_DIGITS+18> s;
           // Note:  14 is a  magic constant from the decNumber library documentation,
           //        the max number of extra characters beyond the number of digits 
           //        needed to represent the number in string form.  Add a few more
           //        for the additional digits we retain.

        // Round down to appx. double precision, if the number is longer than that.
        // Copy the number first, so that we don't modify the original.
        if (getCount() > MAX_DBL_DIGITS + 3) {
            DigitList numToConvert(*this);
            numToConvert.reduce();    // Removes any trailing zeros, so that digit count is good.
            numToConvert.round(MAX_DBL_DIGITS+3);
            uprv_decNumberToString(numToConvert.fDecNumber, s.getAlias());
            // TODO:  how many extra digits should be included for an accurate conversion?
        } else {
            uprv_decNumberToString(this->fDecNumber, s.getAlias());
        }
        U_ASSERT(uprv_strlen(&s[0]) < MAX_DBL_DIGITS+18);

        char *end = NULL;
        tDouble = decimalStrToDouble(s.getAlias(), &end);
    }
    {
        Mutex mutex;
        DigitList *nonConstThis = const_cast<DigitList *>(this);
        nonConstThis->internalSetDouble(tDouble);
    }
    return tDouble;
}

#if U_USE_STRTOD_L && U_PLATFORM_USES_ONLY_WIN32_API
# define locale_t _locale_t
# define freelocale _free_locale
# define strtod_l _strtod_l
#endif

#if U_USE_STRTOD_L
static locale_t gCLocale = (locale_t)0;
#endif
static icu::UInitOnce gCLocaleInitOnce = U_INITONCE_INITIALIZER;

U_CDECL_BEGIN
// Cleanup callback func
static UBool U_CALLCONV digitList_cleanup(void)
{
#if U_USE_STRTOD_L
    if (gCLocale != (locale_t)0) {
        freelocale(gCLocale);
    }
#endif
    return TRUE;
}
// C Locale initialization func
static void U_CALLCONV initCLocale(void) {
    ucln_i18n_registerCleanup(UCLN_I18N_DIGITLIST, digitList_cleanup);
#if U_USE_STRTOD_L
# if U_PLATFORM_USES_ONLY_WIN32_API
    gCLocale = _create_locale(LC_ALL, "C");
# else
    gCLocale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
# endif
#endif
}
U_CDECL_END

double
DigitList::decimalStrToDouble(char *decstr, char **end) {
    umtx_initOnce(gCLocaleInitOnce, &initCLocale);
#if U_USE_STRTOD_L
    return strtod_l(decstr, end, gCLocale);
#else
    char *decimalPt = strchr(decstr, '.');
    if (decimalPt) {
        // We need to know the decimal separator character that will be used with strtod().
        // Depends on the C runtime global locale.
        // Most commonly is '.'
        char rep[MAX_DIGITS];
        sprintf(rep, "%+1.1f", 1.0);
        *decimalPt = rep[2];
    }
    return uprv_strtod(decstr, end);
#endif
}

// -------------------------------------

/**
 *  convert this number to an int32_t.   Round if there is a fractional part.
 *  Return zero if the number cannot be represented.
 */
int32_t DigitList::getLong() /*const*/
{
    int32_t result = 0;
    if (getUpperExponent() > 10) { 
        // Overflow, absolute value too big.
        return result;
    }
    if (fDecNumber->exponent != 0) {
        // Force to an integer, with zero exponent, rounding if necessary.
        //   (decNumberToInt32 will only work if the exponent is exactly zero.)
        DigitList copy(*this);
        DigitList zero;
        uprv_decNumberQuantize(copy.fDecNumber, copy.fDecNumber, zero.fDecNumber, &fContext);
        result = uprv_decNumberToInt32(copy.fDecNumber, &fContext);
    } else {
        result = uprv_decNumberToInt32(fDecNumber, &fContext);
    }
    return result;
}


/**
 *  convert this number to an int64_t.   Truncate if there is a fractional part.
 *  Return zero if the number cannot be represented.
 */
int64_t DigitList::getInt64() /*const*/ {
    // TODO: fast conversion if fHave == fDouble

    // Truncate if non-integer.
    // Return 0 if out of range.
    // Range of in64_t is -9223372036854775808 to 9223372036854775807  (19 digits)
    //
    if (getUpperExponent() > 19) { 
        // Overflow, absolute value too big.
        return 0;
    }

    // The number of integer digits may differ from the number of digits stored
    //   in the decimal number.
    //     for 12.345  numIntDigits = 2, number->digits = 5
    //     for 12E4    numIntDigits = 6, number->digits = 2
    // The conversion ignores the fraction digits in the first case,
    // and fakes up extra zero digits in the second.
    // TODO:  It would be faster to store a table of powers of ten to multiply by
    //        instead of looping over zero digits, multiplying each time.

    int32_t numIntDigits = getUpperExponent(); 
    uint64_t value = 0;
    for (int32_t i = 0; i < numIntDigits; i++) {
        // Loop is iterating over digits starting with the most significant.
        // Numbers are stored with the least significant digit at index zero.
        int32_t digitIndex = fDecNumber->digits - i - 1;
        int32_t v = (digitIndex >= 0) ? fDecNumber->lsu[digitIndex] : 0;
        value = value * (uint64_t)10 + (uint64_t)v;
    }

    if (decNumberIsNegative(fDecNumber)) {
        value = ~value;
        value += 1;
    }
    int64_t svalue = (int64_t)value;

    // Check overflow.  It's convenient that the MSD is 9 only on overflow, the amount of
    //                  overflow can't wrap too far.  The test will also fail -0, but
    //                  that does no harm; the right answer is 0.
    if (numIntDigits == 19) {
        if (( decNumberIsNegative(fDecNumber) && svalue>0) ||
            (!decNumberIsNegative(fDecNumber) && svalue<0)) {
            svalue = 0;
        }
    }
        
    return svalue;
}


/**
 *  Return a string form of this number.
 *     Format is as defined by the decNumber library, for interchange of
 *     decimal numbers.
 */
void DigitList::getDecimal(CharString &str, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    // A decimal number in string form can, worst case, be 14 characters longer
    //  than the number of digits.  So says the decNumber library doc.
    int32_t maxLength = fDecNumber->digits + 14;
    int32_t capacity = 0;
    char *buffer = str.clear().getAppendBuffer(maxLength, 0, capacity, status);
    if (U_FAILURE(status)) {
        return;    // Memory allocation error on growing the string.
    }
    U_ASSERT(capacity >= maxLength);
    uprv_decNumberToString(this->fDecNumber, buffer);
    U_ASSERT((int32_t)uprv_strlen(buffer) <= maxLength);
    str.append(buffer, -1, status);
}

/**
 * Return true if this is an integer value that can be held
 * by an int32_t type.
 */
UBool
DigitList::fitsIntoLong(UBool ignoreNegativeZero) /*const*/
{
    if (decNumberIsSpecial(this->fDecNumber)) {
        // NaN or Infinity.  Does not fit in int32.
        return FALSE;
    }
    uprv_decNumberTrim(this->fDecNumber);
    if (fDecNumber->exponent < 0) {
        // Number contains fraction digits.
        return FALSE;
    }
    if (decNumberIsZero(this->fDecNumber) && !ignoreNegativeZero &&
        (fDecNumber->bits & DECNEG) != 0) {
        // Negative Zero, not ingored.  Cannot represent as a long.
        return FALSE;
    }
    if (getUpperExponent() < 10) { 
        // The number is 9 or fewer digits.
        // The max and min int32 are 10 digts, so this number fits.
        // This is the common case.
        return TRUE;
    }

    // TODO:  Should cache these constants; construction is relatively costly.
    //        But not of huge consequence; they're only needed for 10 digit ints.
    UErrorCode status = U_ZERO_ERROR;
    DigitList min32; min32.set("-2147483648", status);
    if (this->compare(min32) < 0) {
        return FALSE;
    }
    DigitList max32; max32.set("2147483647", status);
    if (this->compare(max32) > 0) {
        return FALSE;
    }
    if (U_FAILURE(status)) {
        return FALSE;
    }
    return true;
}



/**
 * Return true if the number represented by this object can fit into
 * a long.
 */
UBool
DigitList::fitsIntoInt64(UBool ignoreNegativeZero) /*const*/
{
    if (decNumberIsSpecial(this->fDecNumber)) {
        // NaN or Infinity.  Does not fit in int32.
        return FALSE;
    }
    uprv_decNumberTrim(this->fDecNumber);
    if (fDecNumber->exponent < 0) {
        // Number contains fraction digits.
        return FALSE;
    }
    if (decNumberIsZero(this->fDecNumber) && !ignoreNegativeZero &&
        (fDecNumber->bits & DECNEG) != 0) {
        // Negative Zero, not ingored.  Cannot represent as a long.
        return FALSE;
    }
    if (getUpperExponent() < 19) { 
        // The number is 18 or fewer digits.
        // The max and min int64 are 19 digts, so this number fits.
        // This is the common case.
        return TRUE;
    }

    // TODO:  Should cache these constants; construction is relatively costly.
    //        But not of huge consequence; they're only needed for 19 digit ints.
    UErrorCode status = U_ZERO_ERROR;
    DigitList min64; min64.set("-9223372036854775808", status);
    if (this->compare(min64) < 0) {
        return FALSE;
    }
    DigitList max64; max64.set("9223372036854775807", status);
    if (this->compare(max64) > 0) {
        return FALSE;
    }
    if (U_FAILURE(status)) {
        return FALSE;
    }
    return true;
}


// -------------------------------------

void
DigitList::set(int32_t source)
{
    set((int64_t)source);
    internalSetDouble(source);
}

// -------------------------------------
/**
 * Set an int64, via decnumber
 */
void
DigitList::set(int64_t source)
{
    char str[MAX_DIGITS+2];   // Leave room for sign and trailing nul.
    formatBase10(source, str);
    U_ASSERT(uprv_strlen(str) < sizeof(str));

    uprv_decNumberFromString(fDecNumber, str, &fContext);
    internalSetDouble(static_cast<double>(source));
}

// -------------------------------------
/**
 * Set the DigitList from a decimal number string.
 *
 * The incoming string _must_ be nul terminated, even though it is arriving
 * as a StringPiece because that is what the decNumber library wants.
 * We can get away with this for an internal function; it would not
 * be acceptable for a public API.
 */
void
DigitList::set(StringPiece source, UErrorCode &status, uint32_t /*fastpathBits*/) {
    if (U_FAILURE(status)) {
        return;
    }

#if 0    
    if(fastpathBits==(kFastpathOk|kNoDecimal)) {
      int32_t size = source.size();
      const char *data = source.data();
      int64_t r = 0;
      int64_t m = 1;
      // fast parse
      while(size>0) {
        char ch = data[--size];
        if(ch=='+') {
          break;
        } else if(ch=='-') {
          r = -r;
          break;
        } else {
          int64_t d = ch-'0';
          //printf("CH[%d]=%c, %d, *=%d\n", size,ch, (int)d, (int)m);
          r+=(d)*m;
          m *= 10;
        }
      }
      //printf("R=%d\n", r);
      set(r);
    } else
#endif
        {
      // Figure out a max number of digits to use during the conversion, and
      // resize the number up if necessary.
      int32_t numDigits = source.length();
      if (numDigits > fContext.digits) {
        // fContext.digits == fStorage.getCapacity()
        decNumber *t = fStorage.resize(numDigits, fStorage.getCapacity());
        if (t == NULL) {
          status = U_MEMORY_ALLOCATION_ERROR;
          return;
        }
        fDecNumber = t;
        fContext.digits = numDigits;
      }

      fContext.status = 0;
      uprv_decNumberFromString(fDecNumber, source.data(), &fContext);
      if ((fContext.status & DEC_Conversion_syntax) != 0) {
        status = U_DECIMAL_NUMBER_SYNTAX_ERROR;
      }
    }
    internalClear();
}   

/**
 * Set the digit list to a representation of the given double value.
 * This method supports both fixed-point and exponential notation.
 * @param source Value to be converted.
 */
void
DigitList::set(double source)
{
    // for now, simple implementation; later, do proper IEEE stuff
    char rep[MAX_DIGITS + 8]; // Extra space for '+', '.', e+NNN, and '\0' (actually +8 is enough)

    // Generate a representation of the form /[+-][0-9].[0-9]+e[+-][0-9]+/
    // Can also generate /[+-]nan/ or /[+-]inf/
    // TODO: Use something other than sprintf() here, since it's behavior is somewhat platform specific.
    //       That is why infinity is special cased here.
    if (uprv_isInfinite(source)) {
        if (uprv_isNegativeInfinity(source)) {
            uprv_strcpy(rep,"-inf"); // Handle negative infinity
        } else {
            uprv_strcpy(rep,"inf");
        }
    } else {
        sprintf(rep, "%+1.*e", MAX_DBL_DIGITS - 1, source);
    }
    U_ASSERT(uprv_strlen(rep) < sizeof(rep));

    // uprv_decNumberFromString() will parse the string expecting '.' as a
    // decimal separator, however sprintf() can use ',' in certain locales.
    // Overwrite a ',' with '.' here before proceeding.
    char *decimalSeparator = strchr(rep, ',');
    if (decimalSeparator != NULL) {
        *decimalSeparator = '.';
    }

    // Create a decNumber from the string.
    uprv_decNumberFromString(fDecNumber, rep, &fContext);
    uprv_decNumberTrim(fDecNumber);
    internalSetDouble(source);
}

// -------------------------------------

/*
 * Multiply
 *      The number will be expanded if need be to retain full precision.
 *      In practice, for formatting, multiply is by 10, 100 or 1000, so more digits
 *      will not be required for this use.
 */
void
DigitList::mult(const DigitList &other, UErrorCode &status) {
    if (U_FAILURE(status)) { 
        return; 
    } 
    fContext.status = 0;
    int32_t requiredDigits = this->digits() + other.digits();
    if (requiredDigits > fContext.digits) {
        reduce();    // Remove any trailing zeros
        int32_t requiredDigits = this->digits() + other.digits();
        ensureCapacity(requiredDigits, status);
    }
    uprv_decNumberMultiply(fDecNumber, fDecNumber, other.fDecNumber, &fContext);
    internalClear();
}

// -------------------------------------

/*
 * Divide
 *      The number will _not_ be expanded for inexact results.
 *      TODO:  probably should expand some, for rounding increments that
 *             could add a few digits, e.g. .25, but not expand arbitrarily.
 */
void
DigitList::div(const DigitList &other, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    uprv_decNumberDivide(fDecNumber, fDecNumber, other.fDecNumber, &fContext);
    internalClear();
}

// -------------------------------------

/*
 * ensureCapacity.   Grow the digit storage for the number if it's less than the requested
 *         amount.  Never reduce it.  Available size is kept in fContext.digits.
 */
void
DigitList::ensureCapacity(int32_t requestedCapacity, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (requestedCapacity <= 0) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if (requestedCapacity > DEC_MAX_DIGITS) {
        // Don't report an error for requesting too much.
        // Arithemetic Results will be rounded to what can be supported.
        //   At 999,999,999 max digits, exceeding the limit is not too likely!
        requestedCapacity = DEC_MAX_DIGITS;
    }
    if (requestedCapacity > fContext.digits) {
        decNumber *newBuffer = fStorage.resize(requestedCapacity, fStorage.getCapacity());
        if (newBuffer == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        fContext.digits = requestedCapacity;
        fDecNumber = newBuffer;
    }
}

// -------------------------------------

/**
 * Round the representation to the given number of digits.
 * @param maximumDigits The maximum number of digits to be shown.
 * Upon return, count will be less than or equal to maximumDigits.
 */
void
DigitList::round(int32_t maximumDigits)
{
    reduce(); 
    if (maximumDigits >= fDecNumber->digits) { 
        return; 
    } 
    int32_t savedDigits  = fContext.digits;
    fContext.digits = maximumDigits;
    uprv_decNumberPlus(fDecNumber, fDecNumber, &fContext);
    fContext.digits = savedDigits;
    uprv_decNumberTrim(fDecNumber);
    reduce(); 
    internalClear();
}


void
DigitList::roundFixedPoint(int32_t maximumFractionDigits) {
    reduce();        // Remove trailing zeros. 
    if (fDecNumber->exponent >= -maximumFractionDigits) {
        return;
    }
    decNumber scale;   // Dummy decimal number, but with the desired number of
    uprv_decNumberZero(&scale);    //    fraction digits.
    scale.exponent = -maximumFractionDigits;
    scale.lsu[0] = 1;
    
    uprv_decNumberQuantize(fDecNumber, fDecNumber, &scale, &fContext);
    reduce(); 
    internalClear();
}

// -------------------------------------

void
DigitList::toIntegralValue() {
    uprv_decNumberToIntegralValue(fDecNumber, fDecNumber, &fContext);
}


// -------------------------------------
UBool
DigitList::isZero() const
{
    return decNumberIsZero(fDecNumber);
}

// -------------------------------------
int32_t
DigitList::getUpperExponent() const {
    return fDecNumber->digits + fDecNumber->exponent;
}

DigitInterval &
DigitList::getSmallestInterval(DigitInterval &result) const {
    result.setLeastSignificantInclusive(fDecNumber->exponent);
    result.setMostSignificantExclusive(getUpperExponent());
    return result;
}

uint8_t
DigitList::getDigitByExponent(int32_t exponent) const {
    int32_t idx = exponent - fDecNumber->exponent;
    if (idx < 0 || idx >= fDecNumber->digits) {
        return 0;
    }
    return fDecNumber->lsu[idx];
}

void
DigitList::appendDigitsTo(CharString &str, UErrorCode &status) const {
    str.append((const char *) fDecNumber->lsu, fDecNumber->digits, status);
}

void
DigitList::roundAtExponent(int32_t exponent, int32_t maxSigDigits) {
    reduce();
    if (maxSigDigits < fDecNumber->digits) {
        int32_t minExponent = getUpperExponent() - maxSigDigits;
        if (exponent < minExponent) {
            exponent = minExponent;
        }
    }
    if (exponent <= fDecNumber->exponent) {
        return;
    }
    int32_t digits = getUpperExponent() - exponent;
    if (digits > 0) {
        round(digits);
    } else {
        roundFixedPoint(-exponent);
    }
}

void
DigitList::quantize(const DigitList &quantity, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    div(quantity, status);
    roundAtExponent(0);
    mult(quantity, status);
    reduce();
}

int32_t
DigitList::getScientificExponent(
        int32_t minIntDigitCount, int32_t exponentMultiplier) const {
    // The exponent for zero is always zero.
    if (isZero()) {
        return 0;
    }
    int32_t intDigitCount = getUpperExponent();
    int32_t exponent;
    if (intDigitCount >= minIntDigitCount) {
        int32_t maxAdjustment = intDigitCount - minIntDigitCount;
        exponent = (maxAdjustment / exponentMultiplier) * exponentMultiplier;
    } else {
        int32_t minAdjustment = minIntDigitCount - intDigitCount;
        exponent = ((minAdjustment + exponentMultiplier - 1) / exponentMultiplier) * -exponentMultiplier;
    }
    return exponent;
}

int32_t
DigitList::toScientific(
        int32_t minIntDigitCount, int32_t exponentMultiplier) {
    int32_t exponent = getScientificExponent(
            minIntDigitCount, exponentMultiplier);
    shiftDecimalRight(-exponent);
    return exponent;
}

void
DigitList::shiftDecimalRight(int32_t n) {
    fDecNumber->exponent += n;
    internalClear();
}

U_NAMESPACE_END
#endif // #if !UCONFIG_NO_FORMATTING

//eof
