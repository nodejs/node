// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/numbers/cached-powers.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>

#include <cmath>

#include "src/base/logging.h"

namespace v8 {
namespace base {

struct CachedPower {
  uint64_t significand;
  int16_t binary_exponent;
  int16_t decimal_exponent;
};

static const CachedPower kCachedPowers[] = {
    {0xFA8F'D5A0'081C'0288, -1220, -348}, {0xBAAE'E17F'A23E'BF76, -1193, -340},
    {0x8B16'FB20'3055'AC76, -1166, -332}, {0xCF42'894A'5DCE'35EA, -1140, -324},
    {0x9A6B'B0AA'5565'3B2D, -1113, -316}, {0xE61A'CF03'3D1A'45DF, -1087, -308},
    {0xAB70'FE17'C79A'C6CA, -1060, -300}, {0xFF77'B1FC'BEBC'DC4F, -1034, -292},
    {0xBE56'91EF'416B'D60C, -1007, -284}, {0x8DD0'1FAD'907F'FC3C, -980, -276},
    {0xD351'5C28'3155'9A83, -954, -268},  {0x9D71'AC8F'ADA6'C9B5, -927, -260},
    {0xEA9C'2277'23EE'8BCB, -901, -252},  {0xAECC'4991'4078'536D, -874, -244},
    {0x823C'1279'5DB6'CE57, -847, -236},  {0xC210'9436'4DFB'5637, -821, -228},
    {0x9096'EA6F'3848'984F, -794, -220},  {0xD774'85CB'2582'3AC7, -768, -212},
    {0xA086'CFCD'97BF'97F4, -741, -204},  {0xEF34'0A98'172A'ACE5, -715, -196},
    {0xB238'67FB'2A35'B28E, -688, -188},  {0x84C8'D4DF'D2C6'3F3B, -661, -180},
    {0xC5DD'4427'1AD3'CDBA, -635, -172},  {0x936B'9FCE'BB25'C996, -608, -164},
    {0xDBAC'6C24'7D62'A584, -582, -156},  {0xA3AB'6658'0D5F'DAF6, -555, -148},
    {0xF3E2'F893'DEC3'F126, -529, -140},  {0xB5B5'ADA8'AAFF'80B8, -502, -132},
    {0x8762'5F05'6C7C'4A8B, -475, -124},  {0xC9BC'FF60'34C1'3053, -449, -116},
    {0x964E'858C'91BA'2655, -422, -108},  {0xDFF9'7724'7029'7EBD, -396, -100},
    {0xA6DF'BD9F'B8E5'B88F, -369, -92},   {0xF8A9'5FCF'8874'7D94, -343, -84},
    {0xB944'7093'8FA8'9BCF, -316, -76},   {0x8A08'F0F8'BF0F'156B, -289, -68},
    {0xCDB0'2555'6531'31B6, -263, -60},   {0x993F'E2C6'D07B'7FAC, -236, -52},
    {0xE45C'10C4'2A2B'3B06, -210, -44},   {0xAA24'2499'6973'92D3, -183, -36},
    {0xFD87'B5F2'8300'CA0E, -157, -28},   {0xBCE5'0864'9211'1AEB, -130, -20},
    {0x8CBC'CC09'6F50'88CC, -103, -12},   {0xD1B7'1758'E219'652C, -77, -4},
    {0x9C40'0000'0000'0000, -50, 4},      {0xE8D4'A510'0000'0000, -24, 12},
    {0xAD78'EBC5'AC62'0000, 3, 20},       {0x813F'3978'F894'0984, 30, 28},
    {0xC097'CE7B'C907'15B3, 56, 36},      {0x8F7E'32CE'7BEA'5C70, 83, 44},
    {0xD5D2'38A4'ABE9'8068, 109, 52},     {0x9F4F'2726'179A'2245, 136, 60},
    {0xED63'A231'D4C4'FB27, 162, 68},     {0xB0DE'6538'8CC8'ADA8, 189, 76},
    {0x83C7'088E'1AAB'65DB, 216, 84},     {0xC45D'1DF9'4271'1D9A, 242, 92},
    {0x924D'692C'A61B'E758, 269, 100},    {0xDA01'EE64'1A70'8DEA, 295, 108},
    {0xA26D'A399'9AEF'774A, 322, 116},    {0xF209'787B'B47D'6B85, 348, 124},
    {0xB454'E4A1'79DD'1877, 375, 132},    {0x865B'8692'5B9B'C5C2, 402, 140},
    {0xC835'53C5'C896'5D3D, 428, 148},    {0x952A'B45C'FA97'A0B3, 455, 156},
    {0xDE46'9FBD'99A0'5FE3, 481, 164},    {0xA59B'C234'DB39'8C25, 508, 172},
    {0xF6C6'9A72'A398'9F5C, 534, 180},    {0xB7DC'BF53'54E9'BECE, 561, 188},
    {0x88FC'F317'F222'41E2, 588, 196},    {0xCC20'CE9B'D35C'78A5, 614, 204},
    {0x9816'5AF3'7B21'53DF, 641, 212},    {0xE2A0'B5DC'971F'303A, 667, 220},
    {0xA8D9'D153'5CE3'B396, 694, 228},    {0xFB9B'7CD9'A4A7'443C, 720, 236},
    {0xBB76'4C4C'A7A4'4410, 747, 244},    {0x8BAB'8EEF'B640'9C1A, 774, 252},
    {0xD01F'EF10'A657'842C, 800, 260},    {0x9B10'A4E5'E991'3129, 827, 268},
    {0xE710'9BFB'A19C'0C9D, 853, 276},    {0xAC28'20D9'623B'F429, 880, 284},
    {0x8044'4B5E'7AA7'CF85, 907, 292},    {0xBF21'E440'03AC'DD2D, 933, 300},
    {0x8E67'9C2F'5E44'FF8F, 960, 308},    {0xD433'179D'9C8C'B841, 986, 316},
    {0x9E19'DB92'B4E3'1BA9, 1013, 324},   {0xEB96'BF6E'BADF'77D9, 1039, 332},
    {0xAF87'023B'9BF0'EE6B, 1066, 340},
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
    int min_exponent, int max_exponent, DiyFp* power, int* decimal_exponent) {
  int kQ = DiyFp::kSignificandSize;
  // Some platforms return incorrect sign on 0 result. We can ignore that here,
  // which means we can avoid depending on platform.h.
  double k = std::ceil((min_exponent + kQ - 1) * kD_1_LOG2_10);
  int foo = kCachedPowersOffset;
  int index = (foo + static_cast<int>(k) - 1) / kDecimalExponentDistance + 1;
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

}  // namespace base
}  // namespace v8
