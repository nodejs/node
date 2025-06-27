// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
// From the double-conversion library. Original license:
//
// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ICU PATCH: ifdef around UCONFIG_NO_FORMATTING
#include "unicode/utypes.h"
#if !UCONFIG_NO_FORMATTING

#include <cmath>

// ICU PATCH: Customize header file paths for ICU.

#include "double-conversion-bignum-dtoa.h"

#include "double-conversion-bignum.h"
#include "double-conversion-ieee.h"

// ICU PATCH: Wrap in ICU namespace
U_NAMESPACE_BEGIN

namespace double_conversion {

static int NormalizedExponent(uint64_t significand, int exponent) {
  DOUBLE_CONVERSION_ASSERT(significand != 0);
  while ((significand & Double::kHiddenBit) == 0) {
    significand = significand << 1;
    exponent = exponent - 1;
  }
  return exponent;
}


// Forward declarations:
// Returns an estimation of k such that 10^(k-1) <= v < 10^k.
static int EstimatePower(int exponent);
// Computes v / 10^estimated_power exactly, as a ratio of two bignums, numerator
// and denominator.
static void InitialScaledStartValues(uint64_t significand,
                                     int exponent,
                                     bool lower_boundary_is_closer,
                                     int estimated_power,
                                     bool need_boundary_deltas,
                                     Bignum* numerator,
                                     Bignum* denominator,
                                     Bignum* delta_minus,
                                     Bignum* delta_plus);
// Multiplies numerator/denominator so that its values lies in the range 1-10.
// Returns decimal_point s.t.
//  v = numerator'/denominator' * 10^(decimal_point-1)
//     where numerator' and denominator' are the values of numerator and
//     denominator after the call to this function.
static void FixupMultiply10(int estimated_power, bool is_even,
                            int* decimal_point,
                            Bignum* numerator, Bignum* denominator,
                            Bignum* delta_minus, Bignum* delta_plus);
// Generates digits from the left to the right and stops when the generated
// digits yield the shortest decimal representation of v.
static void GenerateShortestDigits(Bignum* numerator, Bignum* denominator,
                                   Bignum* delta_minus, Bignum* delta_plus,
                                   bool is_even,
                                   Vector<char> buffer, int* length);
// Generates 'requested_digits' after the decimal point.
static void BignumToFixed(int requested_digits, int* decimal_point,
                          Bignum* numerator, Bignum* denominator,
                          Vector<char> buffer, int* length);
// Generates 'count' digits of numerator/denominator.
// Once 'count' digits have been produced rounds the result depending on the
// remainder (remainders of exactly .5 round upwards). Might update the
// decimal_point when rounding up (for example for 0.9999).
static void GenerateCountedDigits(int count, int* decimal_point,
                                  Bignum* numerator, Bignum* denominator,
                                  Vector<char> buffer, int* length);


void BignumDtoa(double v, BignumDtoaMode mode, int requested_digits,
                Vector<char> buffer, int* length, int* decimal_point) {
  DOUBLE_CONVERSION_ASSERT(v > 0);
  DOUBLE_CONVERSION_ASSERT(!Double(v).IsSpecial());
  uint64_t significand;
  int exponent;
  bool lower_boundary_is_closer;
  if (mode == BIGNUM_DTOA_SHORTEST_SINGLE) {
    float f = static_cast<float>(v);
    DOUBLE_CONVERSION_ASSERT(f == v);
    significand = Single(f).Significand();
    exponent = Single(f).Exponent();
    lower_boundary_is_closer = Single(f).LowerBoundaryIsCloser();
  } else {
    significand = Double(v).Significand();
    exponent = Double(v).Exponent();
    lower_boundary_is_closer = Double(v).LowerBoundaryIsCloser();
  }
  bool need_boundary_deltas =
      (mode == BIGNUM_DTOA_SHORTEST || mode == BIGNUM_DTOA_SHORTEST_SINGLE);

  bool is_even = (significand & 1) == 0;
  int normalized_exponent = NormalizedExponent(significand, exponent);
  // estimated_power might be too low by 1.
  int estimated_power = EstimatePower(normalized_exponent);

  // Shortcut for Fixed.
  // The requested digits correspond to the digits after the point. If the
  // number is much too small, then there is no need in trying to get any
  // digits.
  if (mode == BIGNUM_DTOA_FIXED && -estimated_power - 1 > requested_digits) {
    buffer[0] = '\0';
    *length = 0;
    // Set decimal-point to -requested_digits. This is what Gay does.
    // Note that it should not have any effect anyways since the string is
    // empty.
    *decimal_point = -requested_digits;
    return;
  }

  Bignum numerator;
  Bignum denominator;
  Bignum delta_minus;
  Bignum delta_plus;
  // Make sure the bignum can grow large enough. The smallest double equals
  // 4e-324. In this case the denominator needs fewer than 324*4 binary digits.
  // The maximum double is 1.7976931348623157e308 which needs fewer than
  // 308*4 binary digits.
  DOUBLE_CONVERSION_ASSERT(Bignum::kMaxSignificantBits >= 324*4);
  InitialScaledStartValues(significand, exponent, lower_boundary_is_closer,
                           estimated_power, need_boundary_deltas,
                           &numerator, &denominator,
                           &delta_minus, &delta_plus);
  // We now have v = (numerator / denominator) * 10^estimated_power.
  FixupMultiply10(estimated_power, is_even, decimal_point,
                  &numerator, &denominator,
                  &delta_minus, &delta_plus);
  // We now have v = (numerator / denominator) * 10^(decimal_point-1), and
  //  1 <= (numerator + delta_plus) / denominator < 10
  switch (mode) {
    case BIGNUM_DTOA_SHORTEST:
    case BIGNUM_DTOA_SHORTEST_SINGLE:
      GenerateShortestDigits(&numerator, &denominator,
                             &delta_minus, &delta_plus,
                             is_even, buffer, length);
      break;
    case BIGNUM_DTOA_FIXED:
      BignumToFixed(requested_digits, decimal_point,
                    &numerator, &denominator,
                    buffer, length);
      break;
    case BIGNUM_DTOA_PRECISION:
      GenerateCountedDigits(requested_digits, decimal_point,
                            &numerator, &denominator,
                            buffer, length);
      break;
    default:
      DOUBLE_CONVERSION_UNREACHABLE();
  }
  buffer[*length] = '\0';
}


// The procedure starts generating digits from the left to the right and stops
// when the generated digits yield the shortest decimal representation of v. A
// decimal representation of v is a number lying closer to v than to any other
// double, so it converts to v when read.
//
// This is true if d, the decimal representation, is between m- and m+, the
// upper and lower boundaries. d must be strictly between them if !is_even.
//           m- := (numerator - delta_minus) / denominator
//           m+ := (numerator + delta_plus) / denominator
//
// Precondition: 0 <= (numerator+delta_plus) / denominator < 10.
//   If 1 <= (numerator+delta_plus) / denominator < 10 then no leading 0 digit
//   will be produced. This should be the standard precondition.
static void GenerateShortestDigits(Bignum* numerator, Bignum* denominator,
                                   Bignum* delta_minus, Bignum* delta_plus,
                                   bool is_even,
                                   Vector<char> buffer, int* length) {
  // Small optimization: if delta_minus and delta_plus are the same just reuse
  // one of the two bignums.
  if (Bignum::Equal(*delta_minus, *delta_plus)) {
    delta_plus = delta_minus;
  }
  *length = 0;
  for (;;) {
    uint16_t digit;
    digit = numerator->DivideModuloIntBignum(*denominator);
    DOUBLE_CONVERSION_ASSERT(digit <= 9);  // digit is a uint16_t and therefore always positive.
    // digit = numerator / denominator (integer division).
    // numerator = numerator % denominator.
    buffer[(*length)++] = static_cast<char>(digit + '0');

    // Can we stop already?
    // If the remainder of the division is less than the distance to the lower
    // boundary we can stop. In this case we simply round down (discarding the
    // remainder).
    // Similarly we test if we can round up (using the upper boundary).
    bool in_delta_room_minus;
    bool in_delta_room_plus;
    if (is_even) {
      in_delta_room_minus = Bignum::LessEqual(*numerator, *delta_minus);
    } else {
      in_delta_room_minus = Bignum::Less(*numerator, *delta_minus);
    }
    if (is_even) {
      in_delta_room_plus =
          Bignum::PlusCompare(*numerator, *delta_plus, *denominator) >= 0;
    } else {
      in_delta_room_plus =
          Bignum::PlusCompare(*numerator, *delta_plus, *denominator) > 0;
    }
    if (!in_delta_room_minus && !in_delta_room_plus) {
      // Prepare for next iteration.
      numerator->Times10();
      delta_minus->Times10();
      // We optimized delta_plus to be equal to delta_minus (if they share the
      // same value). So don't multiply delta_plus if they point to the same
      // object.
      if (delta_minus != delta_plus) {
        delta_plus->Times10();
      }
    } else if (in_delta_room_minus && in_delta_room_plus) {
      // Let's see if 2*numerator < denominator.
      // If yes, then the next digit would be < 5 and we can round down.
      int compare = Bignum::PlusCompare(*numerator, *numerator, *denominator);
      if (compare < 0) {
        // Remaining digits are less than .5. -> Round down (== do nothing).
      } else if (compare > 0) {
        // Remaining digits are more than .5 of denominator. -> Round up.
        // Note that the last digit could not be a '9' as otherwise the whole
        // loop would have stopped earlier.
        // We still have an assert here in case the preconditions were not
        // satisfied.
        DOUBLE_CONVERSION_ASSERT(buffer[(*length) - 1] != '9');
        buffer[(*length) - 1]++;
      } else {
        // Halfway case.
        // TODO(floitsch): need a way to solve half-way cases.
        //   For now let's round towards even (since this is what Gay seems to
        //   do).

        if ((buffer[(*length) - 1] - '0') % 2 == 0) {
          // Round down => Do nothing.
        } else {
          DOUBLE_CONVERSION_ASSERT(buffer[(*length) - 1] != '9');
          buffer[(*length) - 1]++;
        }
      }
      return;
    } else if (in_delta_room_minus) {
      // Round down (== do nothing).
      return;
    } else {  // in_delta_room_plus
      // Round up.
      // Note again that the last digit could not be '9' since this would have
      // stopped the loop earlier.
      // We still have an DOUBLE_CONVERSION_ASSERT here, in case the preconditions were not
      // satisfied.
      DOUBLE_CONVERSION_ASSERT(buffer[(*length) -1] != '9');
      buffer[(*length) - 1]++;
      return;
    }
  }
}


// Let v = numerator / denominator < 10.
// Then we generate 'count' digits of d = x.xxxxx... (without the decimal point)
// from left to right. Once 'count' digits have been produced we decide whether
// to round up or down. Remainders of exactly .5 round upwards. Numbers such
// as 9.999999 propagate a carry all the way, and change the
// exponent (decimal_point), when rounding upwards.
static void GenerateCountedDigits(int count, int* decimal_point,
                                  Bignum* numerator, Bignum* denominator,
                                  Vector<char> buffer, int* length) {
  DOUBLE_CONVERSION_ASSERT(count >= 0);
  for (int i = 0; i < count - 1; ++i) {
    uint16_t digit;
    digit = numerator->DivideModuloIntBignum(*denominator);
    DOUBLE_CONVERSION_ASSERT(digit <= 9);  // digit is a uint16_t and therefore always positive.
    // digit = numerator / denominator (integer division).
    // numerator = numerator % denominator.
    buffer[i] = static_cast<char>(digit + '0');
    // Prepare for next iteration.
    numerator->Times10();
  }
  // Generate the last digit.
  uint16_t digit;
  digit = numerator->DivideModuloIntBignum(*denominator);
  if (Bignum::PlusCompare(*numerator, *numerator, *denominator) >= 0) {
    digit++;
  }
  DOUBLE_CONVERSION_ASSERT(digit <= 10);
  buffer[count - 1] = static_cast<char>(digit + '0');
  // Correct bad digits (in case we had a sequence of '9's). Propagate the
  // carry until we hat a non-'9' or til we reach the first digit.
  for (int i = count - 1; i > 0; --i) {
    if (buffer[i] != '0' + 10) break;
    buffer[i] = '0';
    buffer[i - 1]++;
  }
  if (buffer[0] == '0' + 10) {
    // Propagate a carry past the top place.
    buffer[0] = '1';
    (*decimal_point)++;
  }
  *length = count;
}


// Generates 'requested_digits' after the decimal point. It might omit
// trailing '0's. If the input number is too small then no digits at all are
// generated (ex.: 2 fixed digits for 0.00001).
//
// Input verifies:  1 <= (numerator + delta) / denominator < 10.
static void BignumToFixed(int requested_digits, int* decimal_point,
                          Bignum* numerator, Bignum* denominator,
                          Vector<char> buffer, int* length) {
  // Note that we have to look at more than just the requested_digits, since
  // a number could be rounded up. Example: v=0.5 with requested_digits=0.
  // Even though the power of v equals 0 we can't just stop here.
  if (-(*decimal_point) > requested_digits) {
    // The number is definitively too small.
    // Ex: 0.001 with requested_digits == 1.
    // Set decimal-point to -requested_digits. This is what Gay does.
    // Note that it should not have any effect anyways since the string is
    // empty.
    *decimal_point = -requested_digits;
    *length = 0;
    return;
  } else if (-(*decimal_point) == requested_digits) {
    // We only need to verify if the number rounds down or up.
    // Ex: 0.04 and 0.06 with requested_digits == 1.
    DOUBLE_CONVERSION_ASSERT(*decimal_point == -requested_digits);
    // Initially the fraction lies in range (1, 10]. Multiply the denominator
    // by 10 so that we can compare more easily.
    denominator->Times10();
    if (Bignum::PlusCompare(*numerator, *numerator, *denominator) >= 0) {
      // If the fraction is >= 0.5 then we have to include the rounded
      // digit.
      buffer[0] = '1';
      *length = 1;
      (*decimal_point)++;
    } else {
      // Note that we caught most of similar cases earlier.
      *length = 0;
    }
    return;
  } else {
    // The requested digits correspond to the digits after the point.
    // The variable 'needed_digits' includes the digits before the point.
    int needed_digits = (*decimal_point) + requested_digits;
    GenerateCountedDigits(needed_digits, decimal_point,
                          numerator, denominator,
                          buffer, length);
  }
}


// Returns an estimation of k such that 10^(k-1) <= v < 10^k where
// v = f * 2^exponent and 2^52 <= f < 2^53.
// v is hence a normalized double with the given exponent. The output is an
// approximation for the exponent of the decimal approximation .digits * 10^k.
//
// The result might undershoot by 1 in which case 10^k <= v < 10^k+1.
// Note: this property holds for v's upper boundary m+ too.
//    10^k <= m+ < 10^k+1.
//   (see explanation below).
//
// Examples:
//  EstimatePower(0)   => 16
//  EstimatePower(-52) => 0
//
// Note: e >= 0 => EstimatedPower(e) > 0. No similar claim can be made for e<0.
static int EstimatePower(int exponent) {
  // This function estimates log10 of v where v = f*2^e (with e == exponent).
  // Note that 10^floor(log10(v)) <= v, but v <= 10^ceil(log10(v)).
  // Note that f is bounded by its container size. Let p = 53 (the double's
  // significand size). Then 2^(p-1) <= f < 2^p.
  //
  // Given that log10(v) == log2(v)/log2(10) and e+(len(f)-1) is quite close
  // to log2(v) the function is simplified to (e+(len(f)-1)/log2(10)).
  // The computed number undershoots by less than 0.631 (when we compute log3
  // and not log10).
  //
  // Optimization: since we only need an approximated result this computation
  // can be performed on 64 bit integers. On x86/x64 architecture the speedup is
  // not really measurable, though.
  //
  // Since we want to avoid overshooting we decrement by 1e10 so that
  // floating-point imprecisions don't affect us.
  //
  // Explanation for v's boundary m+: the computation takes advantage of
  // the fact that 2^(p-1) <= f < 2^p. Boundaries still satisfy this requirement
  // (even for denormals where the delta can be much more important).

  const double k1Log10 = 0.30102999566398114;  // 1/lg(10)

  // For doubles len(f) == 53 (don't forget the hidden bit).
  const int kSignificandSize = Double::kSignificandSize;
  double estimate = ceil((exponent + kSignificandSize - 1) * k1Log10 - 1e-10);
  return static_cast<int>(estimate);
}


// See comments for InitialScaledStartValues.
static void InitialScaledStartValuesPositiveExponent(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum* numerator, Bignum* denominator,
    Bignum* delta_minus, Bignum* delta_plus) {
  // A positive exponent implies a positive power.
  DOUBLE_CONVERSION_ASSERT(estimated_power >= 0);
  // Since the estimated_power is positive we simply multiply the denominator
  // by 10^estimated_power.

  // numerator = v.
  numerator->AssignUInt64(significand);
  numerator->ShiftLeft(exponent);
  // denominator = 10^estimated_power.
  denominator->AssignPowerUInt16(10, estimated_power);

  if (need_boundary_deltas) {
    // Introduce a common denominator so that the deltas to the boundaries are
    // integers.
    denominator->ShiftLeft(1);
    numerator->ShiftLeft(1);
    // Let v = f * 2^e, then m+ - v = 1/2 * 2^e; With the common
    // denominator (of 2) delta_plus equals 2^e.
    delta_plus->AssignUInt16(1);
    delta_plus->ShiftLeft(exponent);
    // Same for delta_minus. The adjustments if f == 2^p-1 are done later.
    delta_minus->AssignUInt16(1);
    delta_minus->ShiftLeft(exponent);
  }
}


// See comments for InitialScaledStartValues
static void InitialScaledStartValuesNegativeExponentPositivePower(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum* numerator, Bignum* denominator,
    Bignum* delta_minus, Bignum* delta_plus) {
  // v = f * 2^e with e < 0, and with estimated_power >= 0.
  // This means that e is close to 0 (have a look at how estimated_power is
  // computed).

  // numerator = significand
  //  since v = significand * 2^exponent this is equivalent to
  //  numerator = v * / 2^-exponent
  numerator->AssignUInt64(significand);
  // denominator = 10^estimated_power * 2^-exponent (with exponent < 0)
  denominator->AssignPowerUInt16(10, estimated_power);
  denominator->ShiftLeft(-exponent);

  if (need_boundary_deltas) {
    // Introduce a common denominator so that the deltas to the boundaries are
    // integers.
    denominator->ShiftLeft(1);
    numerator->ShiftLeft(1);
    // Let v = f * 2^e, then m+ - v = 1/2 * 2^e; With the common
    // denominator (of 2) delta_plus equals 2^e.
    // Given that the denominator already includes v's exponent the distance
    // to the boundaries is simply 1.
    delta_plus->AssignUInt16(1);
    // Same for delta_minus. The adjustments if f == 2^p-1 are done later.
    delta_minus->AssignUInt16(1);
  }
}


// See comments for InitialScaledStartValues
static void InitialScaledStartValuesNegativeExponentNegativePower(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum* numerator, Bignum* denominator,
    Bignum* delta_minus, Bignum* delta_plus) {
  // Instead of multiplying the denominator with 10^estimated_power we
  // multiply all values (numerator and deltas) by 10^-estimated_power.

  // Use numerator as temporary container for power_ten.
  Bignum* power_ten = numerator;
  power_ten->AssignPowerUInt16(10, -estimated_power);

  if (need_boundary_deltas) {
    // Since power_ten == numerator we must make a copy of 10^estimated_power
    // before we complete the computation of the numerator.
    // delta_plus = delta_minus = 10^estimated_power
    delta_plus->AssignBignum(*power_ten);
    delta_minus->AssignBignum(*power_ten);
  }

  // numerator = significand * 2 * 10^-estimated_power
  //  since v = significand * 2^exponent this is equivalent to
  // numerator = v * 10^-estimated_power * 2 * 2^-exponent.
  // Remember: numerator has been abused as power_ten. So no need to assign it
  //  to itself.
  DOUBLE_CONVERSION_ASSERT(numerator == power_ten);
  numerator->MultiplyByUInt64(significand);

  // denominator = 2 * 2^-exponent with exponent < 0.
  denominator->AssignUInt16(1);
  denominator->ShiftLeft(-exponent);

  if (need_boundary_deltas) {
    // Introduce a common denominator so that the deltas to the boundaries are
    // integers.
    numerator->ShiftLeft(1);
    denominator->ShiftLeft(1);
    // With this shift the boundaries have their correct value, since
    // delta_plus = 10^-estimated_power, and
    // delta_minus = 10^-estimated_power.
    // These assignments have been done earlier.
    // The adjustments if f == 2^p-1 (lower boundary is closer) are done later.
  }
}


// Let v = significand * 2^exponent.
// Computes v / 10^estimated_power exactly, as a ratio of two bignums, numerator
// and denominator. The functions GenerateShortestDigits and
// GenerateCountedDigits will then convert this ratio to its decimal
// representation d, with the required accuracy.
// Then d * 10^estimated_power is the representation of v.
// (Note: the fraction and the estimated_power might get adjusted before
// generating the decimal representation.)
//
// The initial start values consist of:
//  - a scaled numerator: s.t. numerator/denominator == v / 10^estimated_power.
//  - a scaled (common) denominator.
//  optionally (used by GenerateShortestDigits to decide if it has the shortest
//  decimal converting back to v):
//  - v - m-: the distance to the lower boundary.
//  - m+ - v: the distance to the upper boundary.
//
// v, m+, m-, and therefore v - m- and m+ - v all share the same denominator.
//
// Let ep == estimated_power, then the returned values will satisfy:
//  v / 10^ep = numerator / denominator.
//  v's boundaries m- and m+:
//    m- / 10^ep == v / 10^ep - delta_minus / denominator
//    m+ / 10^ep == v / 10^ep + delta_plus / denominator
//  Or in other words:
//    m- == v - delta_minus * 10^ep / denominator;
//    m+ == v + delta_plus * 10^ep / denominator;
//
// Since 10^(k-1) <= v < 10^k    (with k == estimated_power)
//  or       10^k <= v < 10^(k+1)
//  we then have 0.1 <= numerator/denominator < 1
//           or    1 <= numerator/denominator < 10
//
// It is then easy to kickstart the digit-generation routine.
//
// The boundary-deltas are only filled if the mode equals BIGNUM_DTOA_SHORTEST
// or BIGNUM_DTOA_SHORTEST_SINGLE.

static void InitialScaledStartValues(uint64_t significand,
                                     int exponent,
                                     bool lower_boundary_is_closer,
                                     int estimated_power,
                                     bool need_boundary_deltas,
                                     Bignum* numerator,
                                     Bignum* denominator,
                                     Bignum* delta_minus,
                                     Bignum* delta_plus) {
  if (exponent >= 0) {
    InitialScaledStartValuesPositiveExponent(
        significand, exponent, estimated_power, need_boundary_deltas,
        numerator, denominator, delta_minus, delta_plus);
  } else if (estimated_power >= 0) {
    InitialScaledStartValuesNegativeExponentPositivePower(
        significand, exponent, estimated_power, need_boundary_deltas,
        numerator, denominator, delta_minus, delta_plus);
  } else {
    InitialScaledStartValuesNegativeExponentNegativePower(
        significand, exponent, estimated_power, need_boundary_deltas,
        numerator, denominator, delta_minus, delta_plus);
  }

  if (need_boundary_deltas && lower_boundary_is_closer) {
    // The lower boundary is closer at half the distance of "normal" numbers.
    // Increase the common denominator and adapt all but the delta_minus.
    denominator->ShiftLeft(1);  // *2
    numerator->ShiftLeft(1);    // *2
    delta_plus->ShiftLeft(1);   // *2
  }
}


// This routine multiplies numerator/denominator so that its values lies in the
// range 1-10. That is after a call to this function we have:
//    1 <= (numerator + delta_plus) /denominator < 10.
// Let numerator the input before modification and numerator' the argument
// after modification, then the output-parameter decimal_point is such that
//  numerator / denominator * 10^estimated_power ==
//    numerator' / denominator' * 10^(decimal_point - 1)
// In some cases estimated_power was too low, and this is already the case. We
// then simply adjust the power so that 10^(k-1) <= v < 10^k (with k ==
// estimated_power) but do not touch the numerator or denominator.
// Otherwise the routine multiplies the numerator and the deltas by 10.
static void FixupMultiply10(int estimated_power, bool is_even,
                            int* decimal_point,
                            Bignum* numerator, Bignum* denominator,
                            Bignum* delta_minus, Bignum* delta_plus) {
  bool in_range;
  if (is_even) {
    // For IEEE doubles half-way cases (in decimal system numbers ending with 5)
    // are rounded to the closest floating-point number with even significand.
    in_range = Bignum::PlusCompare(*numerator, *delta_plus, *denominator) >= 0;
  } else {
    in_range = Bignum::PlusCompare(*numerator, *delta_plus, *denominator) > 0;
  }
  if (in_range) {
    // Since numerator + delta_plus >= denominator we already have
    // 1 <= numerator/denominator < 10. Simply update the estimated_power.
    *decimal_point = estimated_power + 1;
  } else {
    *decimal_point = estimated_power;
    numerator->Times10();
    if (Bignum::Equal(*delta_minus, *delta_plus)) {
      delta_minus->Times10();
      delta_plus->AssignBignum(*delta_minus);
    } else {
      delta_minus->Times10();
      delta_plus->Times10();
    }
  }
}

}  // namespace double_conversion

// ICU PATCH: Close ICU namespace
U_NAMESPACE_END
#endif // ICU PATCH: close #if !UCONFIG_NO_FORMATTING
