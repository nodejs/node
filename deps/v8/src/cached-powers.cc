// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/cached-powers.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <cmath>

#include "src/base/logging.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

struct CachedPower {
  uint64_t significand;
  int16_t binary_exponent;
  int16_t decimal_exponent;
};

static const CachedPower kCachedPowers[] = {
    {V8_2PART_UINT64_C(0xFA8FD5A0, 081C0288), -1220, -348},
    {V8_2PART_UINT64_C(0xBAAEE17F, A23EBF76), -1193, -340},
    {V8_2PART_UINT64_C(0x8B16FB20, 3055AC76), -1166, -332},
    {V8_2PART_UINT64_C(0xCF42894A, 5DCE35EA), -1140, -324},
    {V8_2PART_UINT64_C(0x9A6BB0AA, 55653B2D), -1113, -316},
    {V8_2PART_UINT64_C(0xE61ACF03, 3D1A45DF), -1087, -308},
    {V8_2PART_UINT64_C(0xAB70FE17, C79AC6CA), -1060, -300},
    {V8_2PART_UINT64_C(0xFF77B1FC, BEBCDC4F), -1034, -292},
    {V8_2PART_UINT64_C(0xBE5691EF, 416BD60C), -1007, -284},
    {V8_2PART_UINT64_C(0x8DD01FAD, 907FFC3C), -980, -276},
    {V8_2PART_UINT64_C(0xD3515C28, 31559A83), -954, -268},
    {V8_2PART_UINT64_C(0x9D71AC8F, ADA6C9B5), -927, -260},
    {V8_2PART_UINT64_C(0xEA9C2277, 23EE8BCB), -901, -252},
    {V8_2PART_UINT64_C(0xAECC4991, 4078536D), -874, -244},
    {V8_2PART_UINT64_C(0x823C1279, 5DB6CE57), -847, -236},
    {V8_2PART_UINT64_C(0xC2109436, 4DFB5637), -821, -228},
    {V8_2PART_UINT64_C(0x9096EA6F, 3848984F), -794, -220},
    {V8_2PART_UINT64_C(0xD77485CB, 25823AC7), -768, -212},
    {V8_2PART_UINT64_C(0xA086CFCD, 97BF97F4), -741, -204},
    {V8_2PART_UINT64_C(0xEF340A98, 172AACE5), -715, -196},
    {V8_2PART_UINT64_C(0xB23867FB, 2A35B28E), -688, -188},
    {V8_2PART_UINT64_C(0x84C8D4DF, D2C63F3B), -661, -180},
    {V8_2PART_UINT64_C(0xC5DD4427, 1AD3CDBA), -635, -172},
    {V8_2PART_UINT64_C(0x936B9FCE, BB25C996), -608, -164},
    {V8_2PART_UINT64_C(0xDBAC6C24, 7D62A584), -582, -156},
    {V8_2PART_UINT64_C(0xA3AB6658, 0D5FDAF6), -555, -148},
    {V8_2PART_UINT64_C(0xF3E2F893, DEC3F126), -529, -140},
    {V8_2PART_UINT64_C(0xB5B5ADA8, AAFF80B8), -502, -132},
    {V8_2PART_UINT64_C(0x87625F05, 6C7C4A8B), -475, -124},
    {V8_2PART_UINT64_C(0xC9BCFF60, 34C13053), -449, -116},
    {V8_2PART_UINT64_C(0x964E858C, 91BA2655), -422, -108},
    {V8_2PART_UINT64_C(0xDFF97724, 70297EBD), -396, -100},
    {V8_2PART_UINT64_C(0xA6DFBD9F, B8E5B88F), -369, -92},
    {V8_2PART_UINT64_C(0xF8A95FCF, 88747D94), -343, -84},
    {V8_2PART_UINT64_C(0xB9447093, 8FA89BCF), -316, -76},
    {V8_2PART_UINT64_C(0x8A08F0F8, BF0F156B), -289, -68},
    {V8_2PART_UINT64_C(0xCDB02555, 653131B6), -263, -60},
    {V8_2PART_UINT64_C(0x993FE2C6, D07B7FAC), -236, -52},
    {V8_2PART_UINT64_C(0xE45C10C4, 2A2B3B06), -210, -44},
    {V8_2PART_UINT64_C(0xAA242499, 697392D3), -183, -36},
    {V8_2PART_UINT64_C(0xFD87B5F2, 8300CA0E), -157, -28},
    {V8_2PART_UINT64_C(0xBCE50864, 92111AEB), -130, -20},
    {V8_2PART_UINT64_C(0x8CBCCC09, 6F5088CC), -103, -12},
    {V8_2PART_UINT64_C(0xD1B71758, E219652C), -77, -4},
    {V8_2PART_UINT64_C(0x9C400000, 00000000), -50, 4},
    {V8_2PART_UINT64_C(0xE8D4A510, 00000000), -24, 12},
    {V8_2PART_UINT64_C(0xAD78EBC5, AC620000), 3, 20},
    {V8_2PART_UINT64_C(0x813F3978, F8940984), 30, 28},
    {V8_2PART_UINT64_C(0xC097CE7B, C90715B3), 56, 36},
    {V8_2PART_UINT64_C(0x8F7E32CE, 7BEA5C70), 83, 44},
    {V8_2PART_UINT64_C(0xD5D238A4, ABE98068), 109, 52},
    {V8_2PART_UINT64_C(0x9F4F2726, 179A2245), 136, 60},
    {V8_2PART_UINT64_C(0xED63A231, D4C4FB27), 162, 68},
    {V8_2PART_UINT64_C(0xB0DE6538, 8CC8ADA8), 189, 76},
    {V8_2PART_UINT64_C(0x83C7088E, 1AAB65DB), 216, 84},
    {V8_2PART_UINT64_C(0xC45D1DF9, 42711D9A), 242, 92},
    {V8_2PART_UINT64_C(0x924D692C, A61BE758), 269, 100},
    {V8_2PART_UINT64_C(0xDA01EE64, 1A708DEA), 295, 108},
    {V8_2PART_UINT64_C(0xA26DA399, 9AEF774A), 322, 116},
    {V8_2PART_UINT64_C(0xF209787B, B47D6B85), 348, 124},
    {V8_2PART_UINT64_C(0xB454E4A1, 79DD1877), 375, 132},
    {V8_2PART_UINT64_C(0x865B8692, 5B9BC5C2), 402, 140},
    {V8_2PART_UINT64_C(0xC83553C5, C8965D3D), 428, 148},
    {V8_2PART_UINT64_C(0x952AB45C, FA97A0B3), 455, 156},
    {V8_2PART_UINT64_C(0xDE469FBD, 99A05FE3), 481, 164},
    {V8_2PART_UINT64_C(0xA59BC234, DB398C25), 508, 172},
    {V8_2PART_UINT64_C(0xF6C69A72, A3989F5C), 534, 180},
    {V8_2PART_UINT64_C(0xB7DCBF53, 54E9BECE), 561, 188},
    {V8_2PART_UINT64_C(0x88FCF317, F22241E2), 588, 196},
    {V8_2PART_UINT64_C(0xCC20CE9B, D35C78A5), 614, 204},
    {V8_2PART_UINT64_C(0x98165AF3, 7B2153DF), 641, 212},
    {V8_2PART_UINT64_C(0xE2A0B5DC, 971F303A), 667, 220},
    {V8_2PART_UINT64_C(0xA8D9D153, 5CE3B396), 694, 228},
    {V8_2PART_UINT64_C(0xFB9B7CD9, A4A7443C), 720, 236},
    {V8_2PART_UINT64_C(0xBB764C4C, A7A44410), 747, 244},
    {V8_2PART_UINT64_C(0x8BAB8EEF, B6409C1A), 774, 252},
    {V8_2PART_UINT64_C(0xD01FEF10, A657842C), 800, 260},
    {V8_2PART_UINT64_C(0x9B10A4E5, E9913129), 827, 268},
    {V8_2PART_UINT64_C(0xE7109BFB, A19C0C9D), 853, 276},
    {V8_2PART_UINT64_C(0xAC2820D9, 623BF429), 880, 284},
    {V8_2PART_UINT64_C(0x80444B5E, 7AA7CF85), 907, 292},
    {V8_2PART_UINT64_C(0xBF21E440, 03ACDD2D), 933, 300},
    {V8_2PART_UINT64_C(0x8E679C2F, 5E44FF8F), 960, 308},
    {V8_2PART_UINT64_C(0xD433179D, 9C8CB841), 986, 316},
    {V8_2PART_UINT64_C(0x9E19DB92, B4E31BA9), 1013, 324},
    {V8_2PART_UINT64_C(0xEB96BF6E, BADF77D9), 1039, 332},
    {V8_2PART_UINT64_C(0xAF87023B, 9BF0EE6B), 1066, 340},
};

#ifdef DEBUG
static const int kCachedPowersLength = arraysize(kCachedPowers);
#endif

static const int kCachedPowersOffset = 348;  // -1 * the first decimal_exponent.
static const double kD_1_LOG2_10 = 0.30102999566398114;  //  1 / lg(10)
// Difference between the decimal exponents in the table above.
const int PowersOfTenCache::kDecimalExponentDistance = 8;
const int PowersOfTenCache::kMinDecimalExponent = -348;
const int PowersOfTenCache::kMaxDecimalExponent = 340;

void PowersOfTenCache::GetCachedPowerForBinaryExponentRange(
    int min_exponent,
    int max_exponent,
    DiyFp* power,
    int* decimal_exponent) {
  int kQ = DiyFp::kSignificandSize;
  // Some platforms return incorrect sign on 0 result. We can ignore that here,
  // which means we can avoid depending on platform.h.
  double k = std::ceil((min_exponent + kQ - 1) * kD_1_LOG2_10);
  int foo = kCachedPowersOffset;
  int index =
      (foo + static_cast<int>(k) - 1) / kDecimalExponentDistance + 1;
  DCHECK(0 <= index && index < kCachedPowersLength);
  CachedPower cached_power = kCachedPowers[index];
  DCHECK(min_exponent <= cached_power.binary_exponent);
  DCHECK(cached_power.binary_exponent <= max_exponent);
  *decimal_exponent = cached_power.decimal_exponent;
  *power = DiyFp(cached_power.significand, cached_power.binary_exponent);
}


void PowersOfTenCache::GetCachedPowerForDecimalExponent(int requested_exponent,
                                                        DiyFp* power,
                                                        int* found_exponent) {
  DCHECK_LE(kMinDecimalExponent, requested_exponent);
  DCHECK(requested_exponent < kMaxDecimalExponent + kDecimalExponentDistance);
  int index =
      (requested_exponent + kCachedPowersOffset) / kDecimalExponentDistance;
  CachedPower cached_power = kCachedPowers[index];
  *power = DiyFp(cached_power.significand, cached_power.binary_exponent);
  *found_exponent = cached_power.decimal_exponent;
  DCHECK(*found_exponent <= requested_exponent);
  DCHECK(requested_exponent < *found_exponent + kDecimalExponentDistance);
}

}  // namespace internal
}  // namespace v8
