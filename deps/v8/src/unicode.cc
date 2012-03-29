// Copyright 2012 the V8 project authors. All rights reserved.
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
//
// This file was generated at 2012-03-06 09:55:58.934483

#include "unicode-inl.h"
#include <stdlib.h>
#include <stdio.h>

namespace unibrow {

static const int kStartBit = (1 << 30);
static const int kChunkBits = (1 << 13);
static const uchar kSentinel = static_cast<uchar>(-1);

/**
 * \file
 * Implementations of functions for working with unicode.
 */

typedef signed short int16_t;  // NOLINT
typedef unsigned short uint16_t;  // NOLINT
typedef int int32_t;  // NOLINT

// All access to the character table should go through this function.
template <int D>
static inline uchar TableGet(const int32_t* table, int index) {
  return table[D * index];
}

static inline uchar GetEntry(int32_t entry) {
  return entry & (kStartBit - 1);
}

static inline bool IsStart(int32_t entry) {
  return (entry & kStartBit) != 0;
}

/**
 * Look up a character in the unicode table using a mix of binary and
 * interpolation search.  For a uniformly distributed array
 * interpolation search beats binary search by a wide margin.  However,
 * in this case interpolation search degenerates because of some very
 * high values in the lower end of the table so this function uses a
 * combination.  The average number of steps to look up the information
 * about a character is around 10, slightly higher if there is no
 * information available about the character.
 */
static bool LookupPredicate(const int32_t* table, uint16_t size, uchar chr) {
  static const int kEntryDist = 1;
  uint16_t value = chr & (kChunkBits - 1);
  unsigned int low = 0;
  unsigned int high = size - 1;
  while (high != low) {
    unsigned int mid = low + ((high - low) >> 1);
    uchar current_value = GetEntry(TableGet<kEntryDist>(table, mid));
    // If we've found an entry less than or equal to this one, and the
    // next one is not also less than this one, we've arrived.
    if ((current_value <= value) &&
        (mid + 1 == size ||
         GetEntry(TableGet<kEntryDist>(table, mid + 1)) > value)) {
      low = mid;
      break;
    } else if (current_value < value) {
      low = mid + 1;
    } else if (current_value > value) {
      // If we've just checked the bottom-most value and it's not
      // the one we're looking for, we're done.
      if (mid == 0) break;
      high = mid - 1;
    }
  }
  int32_t field = TableGet<kEntryDist>(table, low);
  uchar entry = GetEntry(field);
  bool is_start = IsStart(field);
  return (entry == value) || (entry < value && is_start);
}

template <int kW>
struct MultiCharacterSpecialCase {
  static const uchar kEndOfEncoding = kSentinel;
  uchar chars[kW];
};

// Look up the mapping for the given character in the specified table,
// which is of the specified length and uses the specified special case
// mapping for multi-char mappings.  The next parameter is the character
// following the one to map.  The result will be written in to the result
// buffer and the number of characters written will be returned.  Finally,
// if the allow_caching_ptr is non-null then false will be stored in
// it if the result contains multiple characters or depends on the
// context.
// If ranges are linear, a match between a start and end point is
// offset by the distance between the match and the start. Otherwise
// the result is the same as for the start point on the entire range.
template <bool ranges_are_linear, int kW>
static int LookupMapping(const int32_t* table,
                         uint16_t size,
                         const MultiCharacterSpecialCase<kW>* multi_chars,
                         uchar chr,
                         uchar next,
                         uchar* result,
                         bool* allow_caching_ptr) {
  static const int kEntryDist = 2;
  uint16_t key = chr & (kChunkBits - 1);
  uint16_t chunk_start = chr - key;
  unsigned int low = 0;
  unsigned int high = size - 1;
  while (high != low) {
    unsigned int mid = low + ((high - low) >> 1);
    uchar current_value = GetEntry(TableGet<kEntryDist>(table, mid));
    // If we've found an entry less than or equal to this one, and the next one
    // is not also less than this one, we've arrived.
    if ((current_value <= key) &&
        (mid + 1 == size ||
         GetEntry(TableGet<kEntryDist>(table, mid + 1)) > key)) {
      low = mid;
      break;
    } else if (current_value < key) {
      low = mid + 1;
    } else if (current_value > key) {
      // If we've just checked the bottom-most value and it's not
      // the one we're looking for, we're done.
      if (mid == 0) break;
      high = mid - 1;
    }
  }
  int32_t field = TableGet<kEntryDist>(table, low);
  uchar entry = GetEntry(field);
  bool is_start = IsStart(field);
  bool found = (entry == key) || (entry < key && is_start);
  if (found) {
    int32_t value = table[2 * low + 1];
    if (value == 0) {
      // 0 means not present
      return 0;
    } else if ((value & 3) == 0) {
      // Low bits 0 means a constant offset from the given character.
      if (ranges_are_linear) {
        result[0] = chr + (value >> 2);
      } else {
        result[0] = entry + chunk_start + (value >> 2);
      }
      return 1;
    } else if ((value & 3) == 1) {
      // Low bits 1 means a special case mapping
      if (allow_caching_ptr) *allow_caching_ptr = false;
      const MultiCharacterSpecialCase<kW>& mapping = multi_chars[value >> 2];
      int length = 0;
      for (length = 0; length < kW; length++) {
        uchar mapped = mapping.chars[length];
        if (mapped == MultiCharacterSpecialCase<kW>::kEndOfEncoding) break;
        if (ranges_are_linear) {
          result[length] = mapped + (key - entry);
        } else {
          result[length] = mapped;
        }
      }
      return length;
    } else {
      // Low bits 2 means a really really special case
      if (allow_caching_ptr) *allow_caching_ptr = false;
      // The cases of this switch are defined in unicode.py in the
      // really_special_cases mapping.
      switch (value >> 2) {
        case 1:
          // Really special case 1: upper case sigma.  This letter
          // converts to two different lower case sigmas depending on
          // whether or not it occurs at the end of a word.
          if (next != 0 && Letter::Is(next)) {
            result[0] = 0x03C3;
          } else {
            result[0] = 0x03C2;
          }
          return 1;
        default:
          return 0;
      }
      return -1;
    }
  } else {
    return 0;
  }
}

uchar Utf8::CalculateValue(const byte* str,
                           unsigned length,
                           unsigned* cursor) {
  // We only get called for non-ASCII characters.
  if (length == 1) {
    *cursor += 1;
    return kBadChar;
  }
  byte first = str[0];
  byte second = str[1] ^ 0x80;
  if (second & 0xC0) {
    *cursor += 1;
    return kBadChar;
  }
  if (first < 0xE0) {
    if (first < 0xC0) {
      *cursor += 1;
      return kBadChar;
    }
    uchar code_point = ((first << 6) | second) & kMaxTwoByteChar;
    if (code_point <= kMaxOneByteChar) {
      *cursor += 1;
      return kBadChar;
    }
    *cursor += 2;
    return code_point;
  }
  if (length == 2) {
    *cursor += 1;
    return kBadChar;
  }
  byte third = str[2] ^ 0x80;
  if (third & 0xC0) {
    *cursor += 1;
    return kBadChar;
  }
  if (first < 0xF0) {
    uchar code_point = ((((first << 6) | second) << 6) | third)
        & kMaxThreeByteChar;
    if (code_point <= kMaxTwoByteChar) {
      *cursor += 1;
      return kBadChar;
    }
    *cursor += 3;
    return code_point;
  }
  if (length == 3) {
    *cursor += 1;
    return kBadChar;
  }
  byte fourth = str[3] ^ 0x80;
  if (fourth & 0xC0) {
    *cursor += 1;
    return kBadChar;
  }
  if (first < 0xF8) {
    uchar code_point = (((((first << 6 | second) << 6) | third) << 6) | fourth)
        & kMaxFourByteChar;
    if (code_point <= kMaxThreeByteChar) {
      *cursor += 1;
      return kBadChar;
    }
    *cursor += 4;
    return code_point;
  }
  *cursor += 1;
  return kBadChar;
}


const byte* Utf8::ReadBlock(Buffer<const char*> str, byte* buffer,
    unsigned capacity, unsigned* chars_read_ptr, unsigned* offset_ptr) {
  unsigned offset = *offset_ptr;
  // Bail out early if we've reached the end of the string.
  if (offset == str.length()) {
    *chars_read_ptr = 0;
    return NULL;
  }
  const byte* data = reinterpret_cast<const byte*>(str.data());
  if (data[offset] <= kMaxOneByteChar) {
    // The next character is an ASCII char so we scan forward over
    // the following ASCII characters and return the next pure ASCII
    // substring
    const byte* result = data + offset;
    offset++;
    while ((offset < str.length()) && (data[offset] <= kMaxOneByteChar))
      offset++;
    *chars_read_ptr = offset - *offset_ptr;
    *offset_ptr = offset;
    return result;
  } else {
    // The next character is non-ASCII so we just fill the buffer
    unsigned cursor = 0;
    unsigned chars_read = 0;
    while (offset < str.length()) {
      uchar c = data[offset];
      if (c <= kMaxOneByteChar) {
        // Fast case for ASCII characters
        if (!CharacterStream::EncodeAsciiCharacter(c,
                                                   buffer,
                                                   capacity,
                                                   cursor))
          break;
        offset += 1;
      } else {
        unsigned chars = 0;
        c = Utf8::ValueOf(data + offset, str.length() - offset, &chars);
        if (!CharacterStream::EncodeNonAsciiCharacter(c,
                                                      buffer,
                                                      capacity,
                                                      cursor))
          break;
        offset += chars;
      }
      chars_read++;
    }
    *offset_ptr = offset;
    *chars_read_ptr = chars_read;
    return buffer;
  }
}

unsigned CharacterStream::Length() {
  unsigned result = 0;
  while (has_more()) {
    result++;
    GetNext();
  }
  Rewind();
  return result;
}

unsigned CharacterStream::Utf16Length() {
  unsigned result = 0;
  while (has_more()) {
    uchar c = GetNext();
    result += c > Utf16::kMaxNonSurrogateCharCode ? 2 : 1;
  }
  Rewind();
  return result;
}

void CharacterStream::Seek(unsigned position) {
  Rewind();
  for (unsigned i = 0; i < position; i++) {
    GetNext();
  }
}

// Uppercase:            point.category == 'Lu'

static const uint16_t kUppercaseTable0Size = 450;
static const int32_t kUppercaseTable0[450] = {
  1073741889, 90, 1073742016, 214, 1073742040, 222, 256, 258,  // NOLINT
  260, 262, 264, 266, 268, 270, 272, 274,  // NOLINT
  276, 278, 280, 282, 284, 286, 288, 290,  // NOLINT
  292, 294, 296, 298, 300, 302, 304, 306,  // NOLINT
  308, 310, 313, 315, 317, 319, 321, 323,  // NOLINT
  325, 327, 330, 332, 334, 336, 338, 340,  // NOLINT
  342, 344, 346, 348, 350, 352, 354, 356,  // NOLINT
  358, 360, 362, 364, 366, 368, 370, 372,  // NOLINT
  374, 1073742200, 377, 379, 381, 1073742209, 386, 388,  // NOLINT
  1073742214, 391, 1073742217, 395, 1073742222, 401, 1073742227, 404,  // NOLINT
  1073742230, 408, 1073742236, 413, 1073742239, 416, 418, 420,  // NOLINT
  1073742246, 423, 425, 428, 1073742254, 431, 1073742257, 435,  // NOLINT
  437, 1073742263, 440, 444, 452, 455, 458, 461,  // NOLINT
  463, 465, 467, 469, 471, 473, 475, 478,  // NOLINT
  480, 482, 484, 486, 488, 490, 492, 494,  // NOLINT
  497, 500, 1073742326, 504, 506, 508, 510, 512,  // NOLINT
  514, 516, 518, 520, 522, 524, 526, 528,  // NOLINT
  530, 532, 534, 536, 538, 540, 542, 544,  // NOLINT
  546, 548, 550, 552, 554, 556, 558, 560,  // NOLINT
  562, 1073742394, 571, 1073742397, 574, 577, 1073742403, 582,  // NOLINT
  584, 586, 588, 590, 880, 882, 886, 902,  // NOLINT
  1073742728, 906, 908, 1073742734, 911, 1073742737, 929, 1073742755,  // NOLINT
  939, 975, 1073742802, 980, 984, 986, 988, 990,  // NOLINT
  992, 994, 996, 998, 1000, 1002, 1004, 1006,  // NOLINT
  1012, 1015, 1073742841, 1018, 1073742845, 1071, 1120, 1122,  // NOLINT
  1124, 1126, 1128, 1130, 1132, 1134, 1136, 1138,  // NOLINT
  1140, 1142, 1144, 1146, 1148, 1150, 1152, 1162,  // NOLINT
  1164, 1166, 1168, 1170, 1172, 1174, 1176, 1178,  // NOLINT
  1180, 1182, 1184, 1186, 1188, 1190, 1192, 1194,  // NOLINT
  1196, 1198, 1200, 1202, 1204, 1206, 1208, 1210,  // NOLINT
  1212, 1214, 1073743040, 1217, 1219, 1221, 1223, 1225,  // NOLINT
  1227, 1229, 1232, 1234, 1236, 1238, 1240, 1242,  // NOLINT
  1244, 1246, 1248, 1250, 1252, 1254, 1256, 1258,  // NOLINT
  1260, 1262, 1264, 1266, 1268, 1270, 1272, 1274,  // NOLINT
  1276, 1278, 1280, 1282, 1284, 1286, 1288, 1290,  // NOLINT
  1292, 1294, 1296, 1298, 1300, 1302, 1304, 1306,  // NOLINT
  1308, 1310, 1312, 1314, 1316, 1318, 1073743153, 1366,  // NOLINT
  1073746080, 4293, 4295, 4301, 7680, 7682, 7684, 7686,  // NOLINT
  7688, 7690, 7692, 7694, 7696, 7698, 7700, 7702,  // NOLINT
  7704, 7706, 7708, 7710, 7712, 7714, 7716, 7718,  // NOLINT
  7720, 7722, 7724, 7726, 7728, 7730, 7732, 7734,  // NOLINT
  7736, 7738, 7740, 7742, 7744, 7746, 7748, 7750,  // NOLINT
  7752, 7754, 7756, 7758, 7760, 7762, 7764, 7766,  // NOLINT
  7768, 7770, 7772, 7774, 7776, 7778, 7780, 7782,  // NOLINT
  7784, 7786, 7788, 7790, 7792, 7794, 7796, 7798,  // NOLINT
  7800, 7802, 7804, 7806, 7808, 7810, 7812, 7814,  // NOLINT
  7816, 7818, 7820, 7822, 7824, 7826, 7828, 7838,  // NOLINT
  7840, 7842, 7844, 7846, 7848, 7850, 7852, 7854,  // NOLINT
  7856, 7858, 7860, 7862, 7864, 7866, 7868, 7870,  // NOLINT
  7872, 7874, 7876, 7878, 7880, 7882, 7884, 7886,  // NOLINT
  7888, 7890, 7892, 7894, 7896, 7898, 7900, 7902,  // NOLINT
  7904, 7906, 7908, 7910, 7912, 7914, 7916, 7918,  // NOLINT
  7920, 7922, 7924, 7926, 7928, 7930, 7932, 7934,  // NOLINT
  1073749768, 7951, 1073749784, 7965, 1073749800, 7983, 1073749816, 7999,  // NOLINT
  1073749832, 8013, 8025, 8027, 8029, 8031, 1073749864, 8047,  // NOLINT
  1073749944, 8123, 1073749960, 8139, 1073749976, 8155, 1073749992, 8172,  // NOLINT
  1073750008, 8187 };  // NOLINT
static const uint16_t kUppercaseTable1Size = 86;
static const int32_t kUppercaseTable1[86] = {
  258, 263, 1073742091, 269, 1073742096, 274, 277, 1073742105,  // NOLINT
  285, 292, 294, 296, 1073742122, 301, 1073742128, 307,  // NOLINT
  1073742142, 319, 325, 387, 1073744896, 3118, 3168, 1073744994,  // NOLINT
  3172, 3175, 3177, 3179, 1073745005, 3184, 3186, 3189,  // NOLINT
  1073745022, 3200, 3202, 3204, 3206, 3208, 3210, 3212,  // NOLINT
  3214, 3216, 3218, 3220, 3222, 3224, 3226, 3228,  // NOLINT
  3230, 3232, 3234, 3236, 3238, 3240, 3242, 3244,  // NOLINT
  3246, 3248, 3250, 3252, 3254, 3256, 3258, 3260,  // NOLINT
  3262, 3264, 3266, 3268, 3270, 3272, 3274, 3276,  // NOLINT
  3278, 3280, 3282, 3284, 3286, 3288, 3290, 3292,  // NOLINT
  3294, 3296, 3298, 3307, 3309, 3314 };  // NOLINT
static const uint16_t kUppercaseTable5Size = 91;
static const int32_t kUppercaseTable5[91] = {
  1600, 1602, 1604, 1606, 1608, 1610, 1612, 1614,  // NOLINT
  1616, 1618, 1620, 1622, 1624, 1626, 1628, 1630,  // NOLINT
  1632, 1634, 1636, 1638, 1640, 1642, 1644, 1664,  // NOLINT
  1666, 1668, 1670, 1672, 1674, 1676, 1678, 1680,  // NOLINT
  1682, 1684, 1686, 1826, 1828, 1830, 1832, 1834,  // NOLINT
  1836, 1838, 1842, 1844, 1846, 1848, 1850, 1852,  // NOLINT
  1854, 1856, 1858, 1860, 1862, 1864, 1866, 1868,  // NOLINT
  1870, 1872, 1874, 1876, 1878, 1880, 1882, 1884,  // NOLINT
  1886, 1888, 1890, 1892, 1894, 1896, 1898, 1900,  // NOLINT
  1902, 1913, 1915, 1073743741, 1918, 1920, 1922, 1924,  // NOLINT
  1926, 1931, 1933, 1936, 1938, 1952, 1954, 1956,  // NOLINT
  1958, 1960, 1962 };  // NOLINT
static const uint16_t kUppercaseTable7Size = 2;
static const int32_t kUppercaseTable7[2] = {
  1073749793, 7994 };  // NOLINT
bool Uppercase::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kUppercaseTable0,
                                       kUppercaseTable0Size,
                                       c);
    case 1: return LookupPredicate(kUppercaseTable1,
                                       kUppercaseTable1Size,
                                       c);
    case 5: return LookupPredicate(kUppercaseTable5,
                                       kUppercaseTable5Size,
                                       c);
    case 7: return LookupPredicate(kUppercaseTable7,
                                       kUppercaseTable7Size,
                                       c);
    default: return false;
  }
}

// Lowercase:            point.category == 'Ll'

static const uint16_t kLowercaseTable0Size = 463;
static const int32_t kLowercaseTable0[463] = {
  1073741921, 122, 181, 1073742047, 246, 1073742072, 255, 257,  // NOLINT
  259, 261, 263, 265, 267, 269, 271, 273,  // NOLINT
  275, 277, 279, 281, 283, 285, 287, 289,  // NOLINT
  291, 293, 295, 297, 299, 301, 303, 305,  // NOLINT
  307, 309, 1073742135, 312, 314, 316, 318, 320,  // NOLINT
  322, 324, 326, 1073742152, 329, 331, 333, 335,  // NOLINT
  337, 339, 341, 343, 345, 347, 349, 351,  // NOLINT
  353, 355, 357, 359, 361, 363, 365, 367,  // NOLINT
  369, 371, 373, 375, 378, 380, 1073742206, 384,  // NOLINT
  387, 389, 392, 1073742220, 397, 402, 405, 1073742233,  // NOLINT
  411, 414, 417, 419, 421, 424, 1073742250, 427,  // NOLINT
  429, 432, 436, 438, 1073742265, 442, 1073742269, 447,  // NOLINT
  454, 457, 460, 462, 464, 466, 468, 470,  // NOLINT
  472, 474, 1073742300, 477, 479, 481, 483, 485,  // NOLINT
  487, 489, 491, 493, 1073742319, 496, 499, 501,  // NOLINT
  505, 507, 509, 511, 513, 515, 517, 519,  // NOLINT
  521, 523, 525, 527, 529, 531, 533, 535,  // NOLINT
  537, 539, 541, 543, 545, 547, 549, 551,  // NOLINT
  553, 555, 557, 559, 561, 1073742387, 569, 572,  // NOLINT
  1073742399, 576, 578, 583, 585, 587, 589, 1073742415,  // NOLINT
  659, 1073742485, 687, 881, 883, 887, 1073742715, 893,  // NOLINT
  912, 1073742764, 974, 1073742800, 977, 1073742805, 983, 985,  // NOLINT
  987, 989, 991, 993, 995, 997, 999, 1001,  // NOLINT
  1003, 1005, 1073742831, 1011, 1013, 1016, 1073742843, 1020,  // NOLINT
  1073742896, 1119, 1121, 1123, 1125, 1127, 1129, 1131,  // NOLINT
  1133, 1135, 1137, 1139, 1141, 1143, 1145, 1147,  // NOLINT
  1149, 1151, 1153, 1163, 1165, 1167, 1169, 1171,  // NOLINT
  1173, 1175, 1177, 1179, 1181, 1183, 1185, 1187,  // NOLINT
  1189, 1191, 1193, 1195, 1197, 1199, 1201, 1203,  // NOLINT
  1205, 1207, 1209, 1211, 1213, 1215, 1218, 1220,  // NOLINT
  1222, 1224, 1226, 1228, 1073743054, 1231, 1233, 1235,  // NOLINT
  1237, 1239, 1241, 1243, 1245, 1247, 1249, 1251,  // NOLINT
  1253, 1255, 1257, 1259, 1261, 1263, 1265, 1267,  // NOLINT
  1269, 1271, 1273, 1275, 1277, 1279, 1281, 1283,  // NOLINT
  1285, 1287, 1289, 1291, 1293, 1295, 1297, 1299,  // NOLINT
  1301, 1303, 1305, 1307, 1309, 1311, 1313, 1315,  // NOLINT
  1317, 1319, 1073743201, 1415, 1073749248, 7467, 1073749355, 7543,  // NOLINT
  1073749369, 7578, 7681, 7683, 7685, 7687, 7689, 7691,  // NOLINT
  7693, 7695, 7697, 7699, 7701, 7703, 7705, 7707,  // NOLINT
  7709, 7711, 7713, 7715, 7717, 7719, 7721, 7723,  // NOLINT
  7725, 7727, 7729, 7731, 7733, 7735, 7737, 7739,  // NOLINT
  7741, 7743, 7745, 7747, 7749, 7751, 7753, 7755,  // NOLINT
  7757, 7759, 7761, 7763, 7765, 7767, 7769, 7771,  // NOLINT
  7773, 7775, 7777, 7779, 7781, 7783, 7785, 7787,  // NOLINT
  7789, 7791, 7793, 7795, 7797, 7799, 7801, 7803,  // NOLINT
  7805, 7807, 7809, 7811, 7813, 7815, 7817, 7819,  // NOLINT
  7821, 7823, 7825, 7827, 1073749653, 7837, 7839, 7841,  // NOLINT
  7843, 7845, 7847, 7849, 7851, 7853, 7855, 7857,  // NOLINT
  7859, 7861, 7863, 7865, 7867, 7869, 7871, 7873,  // NOLINT
  7875, 7877, 7879, 7881, 7883, 7885, 7887, 7889,  // NOLINT
  7891, 7893, 7895, 7897, 7899, 7901, 7903, 7905,  // NOLINT
  7907, 7909, 7911, 7913, 7915, 7917, 7919, 7921,  // NOLINT
  7923, 7925, 7927, 7929, 7931, 7933, 1073749759, 7943,  // NOLINT
  1073749776, 7957, 1073749792, 7975, 1073749808, 7991, 1073749824, 8005,  // NOLINT
  1073749840, 8023, 1073749856, 8039, 1073749872, 8061, 1073749888, 8071,  // NOLINT
  1073749904, 8087, 1073749920, 8103, 1073749936, 8116, 1073749942, 8119,  // NOLINT
  8126, 1073749954, 8132, 1073749958, 8135, 1073749968, 8147, 1073749974,  // NOLINT
  8151, 1073749984, 8167, 1073750002, 8180, 1073750006, 8183 };  // NOLINT
static const uint16_t kLowercaseTable1Size = 84;
static const int32_t kLowercaseTable1[84] = {
  266, 1073742094, 271, 275, 303, 308, 313, 1073742140,  // NOLINT
  317, 1073742150, 329, 334, 388, 1073744944, 3166, 3169,  // NOLINT
  1073744997, 3174, 3176, 3178, 3180, 3185, 1073745011, 3188,  // NOLINT
  1073745014, 3195, 3201, 3203, 3205, 3207, 3209, 3211,  // NOLINT
  3213, 3215, 3217, 3219, 3221, 3223, 3225, 3227,  // NOLINT
  3229, 3231, 3233, 3235, 3237, 3239, 3241, 3243,  // NOLINT
  3245, 3247, 3249, 3251, 3253, 3255, 3257, 3259,  // NOLINT
  3261, 3263, 3265, 3267, 3269, 3271, 3273, 3275,  // NOLINT
  3277, 3279, 3281, 3283, 3285, 3287, 3289, 3291,  // NOLINT
  3293, 3295, 3297, 1073745123, 3300, 3308, 3310, 3315,  // NOLINT
  1073745152, 3365, 3367, 3373 };  // NOLINT
static const uint16_t kLowercaseTable5Size = 93;
static const int32_t kLowercaseTable5[93] = {
  1601, 1603, 1605, 1607, 1609, 1611, 1613, 1615,  // NOLINT
  1617, 1619, 1621, 1623, 1625, 1627, 1629, 1631,  // NOLINT
  1633, 1635, 1637, 1639, 1641, 1643, 1645, 1665,  // NOLINT
  1667, 1669, 1671, 1673, 1675, 1677, 1679, 1681,  // NOLINT
  1683, 1685, 1687, 1827, 1829, 1831, 1833, 1835,  // NOLINT
  1837, 1073743663, 1841, 1843, 1845, 1847, 1849, 1851,  // NOLINT
  1853, 1855, 1857, 1859, 1861, 1863, 1865, 1867,  // NOLINT
  1869, 1871, 1873, 1875, 1877, 1879, 1881, 1883,  // NOLINT
  1885, 1887, 1889, 1891, 1893, 1895, 1897, 1899,  // NOLINT
  1901, 1903, 1073743729, 1912, 1914, 1916, 1919, 1921,  // NOLINT
  1923, 1925, 1927, 1932, 1934, 1937, 1939, 1953,  // NOLINT
  1955, 1957, 1959, 1961, 2042 };  // NOLINT
static const uint16_t kLowercaseTable7Size = 6;
static const int32_t kLowercaseTable7[6] = {
  1073748736, 6918, 1073748755, 6935, 1073749825, 8026 };  // NOLINT
bool Lowercase::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kLowercaseTable0,
                                       kLowercaseTable0Size,
                                       c);
    case 1: return LookupPredicate(kLowercaseTable1,
                                       kLowercaseTable1Size,
                                       c);
    case 5: return LookupPredicate(kLowercaseTable5,
                                       kLowercaseTable5Size,
                                       c);
    case 7: return LookupPredicate(kLowercaseTable7,
                                       kLowercaseTable7Size,
                                       c);
    default: return false;
  }
}

// Letter:               point.category in ['Lu', 'Ll', 'Lt', 'Lm', 'Lo', 'Nl' ]

static const uint16_t kLetterTable0Size = 435;
static const int32_t kLetterTable0[435] = {
  1073741889, 90, 1073741921, 122, 170, 181, 186, 1073742016,  // NOLINT
  214, 1073742040, 246, 1073742072, 705, 1073742534, 721, 1073742560,  // NOLINT
  740, 748, 750, 1073742704, 884, 1073742710, 887, 1073742714,  // NOLINT
  893, 902, 1073742728, 906, 908, 1073742734, 929, 1073742755,  // NOLINT
  1013, 1073742839, 1153, 1073742986, 1319, 1073743153, 1366, 1369,  // NOLINT
  1073743201, 1415, 1073743312, 1514, 1073743344, 1522, 1073743392, 1610,  // NOLINT
  1073743470, 1647, 1073743473, 1747, 1749, 1073743589, 1766, 1073743598,  // NOLINT
  1775, 1073743610, 1788, 1791, 1808, 1073743634, 1839, 1073743693,  // NOLINT
  1957, 1969, 1073743818, 2026, 1073743860, 2037, 2042, 1073743872,  // NOLINT
  2069, 2074, 2084, 2088, 1073743936, 2136, 2208, 1073744034,  // NOLINT
  2220, 1073744132, 2361, 2365, 2384, 1073744216, 2401, 1073744241,  // NOLINT
  2423, 1073744249, 2431, 1073744261, 2444, 1073744271, 2448, 1073744275,  // NOLINT
  2472, 1073744298, 2480, 2482, 1073744310, 2489, 2493, 2510,  // NOLINT
  1073744348, 2525, 1073744351, 2529, 1073744368, 2545, 1073744389, 2570,  // NOLINT
  1073744399, 2576, 1073744403, 2600, 1073744426, 2608, 1073744434, 2611,  // NOLINT
  1073744437, 2614, 1073744440, 2617, 1073744473, 2652, 2654, 1073744498,  // NOLINT
  2676, 1073744517, 2701, 1073744527, 2705, 1073744531, 2728, 1073744554,  // NOLINT
  2736, 1073744562, 2739, 1073744565, 2745, 2749, 2768, 1073744608,  // NOLINT
  2785, 1073744645, 2828, 1073744655, 2832, 1073744659, 2856, 1073744682,  // NOLINT
  2864, 1073744690, 2867, 1073744693, 2873, 2877, 1073744732, 2909,  // NOLINT
  1073744735, 2913, 2929, 2947, 1073744773, 2954, 1073744782, 2960,  // NOLINT
  1073744786, 2965, 1073744793, 2970, 2972, 1073744798, 2975, 1073744803,  // NOLINT
  2980, 1073744808, 2986, 1073744814, 3001, 3024, 1073744901, 3084,  // NOLINT
  1073744910, 3088, 1073744914, 3112, 1073744938, 3123, 1073744949, 3129,  // NOLINT
  3133, 1073744984, 3161, 1073744992, 3169, 1073745029, 3212, 1073745038,  // NOLINT
  3216, 1073745042, 3240, 1073745066, 3251, 1073745077, 3257, 3261,  // NOLINT
  3294, 1073745120, 3297, 1073745137, 3314, 1073745157, 3340, 1073745166,  // NOLINT
  3344, 1073745170, 3386, 3389, 3406, 1073745248, 3425, 1073745274,  // NOLINT
  3455, 1073745285, 3478, 1073745306, 3505, 1073745331, 3515, 3517,  // NOLINT
  1073745344, 3526, 1073745409, 3632, 1073745458, 3635, 1073745472, 3654,  // NOLINT
  1073745537, 3714, 3716, 1073745543, 3720, 3722, 3725, 1073745556,  // NOLINT
  3735, 1073745561, 3743, 1073745569, 3747, 3749, 3751, 1073745578,  // NOLINT
  3755, 1073745581, 3760, 1073745586, 3763, 3773, 1073745600, 3780,  // NOLINT
  3782, 1073745628, 3807, 3840, 1073745728, 3911, 1073745737, 3948,  // NOLINT
  1073745800, 3980, 1073745920, 4138, 4159, 1073746000, 4181, 1073746010,  // NOLINT
  4189, 4193, 1073746021, 4198, 1073746030, 4208, 1073746037, 4225,  // NOLINT
  4238, 1073746080, 4293, 4295, 4301, 1073746128, 4346, 1073746172,  // NOLINT
  4680, 1073746506, 4685, 1073746512, 4694, 4696, 1073746522, 4701,  // NOLINT
  1073746528, 4744, 1073746570, 4749, 1073746576, 4784, 1073746610, 4789,  // NOLINT
  1073746616, 4798, 4800, 1073746626, 4805, 1073746632, 4822, 1073746648,  // NOLINT
  4880, 1073746706, 4885, 1073746712, 4954, 1073746816, 5007, 1073746848,  // NOLINT
  5108, 1073746945, 5740, 1073747567, 5759, 1073747585, 5786, 1073747616,  // NOLINT
  5866, 1073747694, 5872, 1073747712, 5900, 1073747726, 5905, 1073747744,  // NOLINT
  5937, 1073747776, 5969, 1073747808, 5996, 1073747822, 6000, 1073747840,  // NOLINT
  6067, 6103, 6108, 1073748000, 6263, 1073748096, 6312, 6314,  // NOLINT
  1073748144, 6389, 1073748224, 6428, 1073748304, 6509, 1073748336, 6516,  // NOLINT
  1073748352, 6571, 1073748417, 6599, 1073748480, 6678, 1073748512, 6740,  // NOLINT
  6823, 1073748741, 6963, 1073748805, 6987, 1073748867, 7072, 1073748910,  // NOLINT
  7087, 1073748922, 7141, 1073748992, 7203, 1073749069, 7247, 1073749082,  // NOLINT
  7293, 1073749225, 7404, 1073749230, 7409, 1073749237, 7414, 1073749248,  // NOLINT
  7615, 1073749504, 7957, 1073749784, 7965, 1073749792, 8005, 1073749832,  // NOLINT
  8013, 1073749840, 8023, 8025, 8027, 8029, 1073749855, 8061,  // NOLINT
  1073749888, 8116, 1073749942, 8124, 8126, 1073749954, 8132, 1073749958,  // NOLINT
  8140, 1073749968, 8147, 1073749974, 8155, 1073749984, 8172, 1073750002,  // NOLINT
  8180, 1073750006, 8188 };  // NOLINT
static const uint16_t kLetterTable1Size = 87;
static const int32_t kLetterTable1[87] = {
  113, 127, 1073741968, 156, 258, 263, 1073742090, 275,  // NOLINT
  277, 1073742105, 285, 292, 294, 296, 1073742122, 301,  // NOLINT
  1073742127, 313, 1073742140, 319, 1073742149, 329, 334, 1073742176,  // NOLINT
  392, 1073744896, 3118, 1073744944, 3166, 1073744992, 3300, 1073745131,  // NOLINT
  3310, 1073745138, 3315, 1073745152, 3365, 3367, 3373, 1073745200,  // NOLINT
  3431, 3439, 1073745280, 3478, 1073745312, 3494, 1073745320, 3502,  // NOLINT
  1073745328, 3510, 1073745336, 3518, 1073745344, 3526, 1073745352, 3534,  // NOLINT
  1073745360, 3542, 1073745368, 3550, 3631, 1073745925, 4103, 1073745953,  // NOLINT
  4137, 1073745969, 4149, 1073745976, 4156, 1073745985, 4246, 1073746077,  // NOLINT
  4255, 1073746081, 4346, 1073746172, 4351, 1073746181, 4397, 1073746225,  // NOLINT
  4494, 1073746336, 4538, 1073746416, 4607, 1073746944, 8191 };  // NOLINT
static const uint16_t kLetterTable2Size = 4;
static const int32_t kLetterTable2[4] = {
  1073741824, 3509, 1073745408, 8191 };  // NOLINT
static const uint16_t kLetterTable3Size = 2;
static const int32_t kLetterTable3[2] = {
  1073741824, 8191 };  // NOLINT
static const uint16_t kLetterTable4Size = 2;
static const int32_t kLetterTable4[2] = {
  1073741824, 8140 };  // NOLINT
static const uint16_t kLetterTable5Size = 88;
static const int32_t kLetterTable5[88] = {
  1073741824, 1164, 1073743056, 1277, 1073743104, 1548, 1073743376, 1567,  // NOLINT
  1073743402, 1579, 1073743424, 1646, 1073743487, 1687, 1073743520, 1775,  // NOLINT
  1073743639, 1823, 1073743650, 1928, 1073743755, 1934, 1073743760, 1939,  // NOLINT
  1073743776, 1962, 1073743864, 2049, 1073743875, 2053, 1073743879, 2058,  // NOLINT
  1073743884, 2082, 1073743936, 2163, 1073744002, 2227, 1073744114, 2295,  // NOLINT
  2299, 1073744138, 2341, 1073744176, 2374, 1073744224, 2428, 1073744260,  // NOLINT
  2482, 2511, 1073744384, 2600, 1073744448, 2626, 1073744452, 2635,  // NOLINT
  1073744480, 2678, 2682, 1073744512, 2735, 2737, 1073744565, 2742,  // NOLINT
  1073744569, 2749, 2752, 2754, 1073744603, 2781, 1073744608, 2794,  // NOLINT
  1073744626, 2804, 1073744641, 2822, 1073744649, 2830, 1073744657, 2838,  // NOLINT
  1073744672, 2854, 1073744680, 2862, 1073744832, 3042, 1073744896, 8191 };  // NOLINT
static const uint16_t kLetterTable6Size = 6;
static const int32_t kLetterTable6[6] = {
  1073741824, 6051, 1073747888, 6086, 1073747915, 6139 };  // NOLINT
static const uint16_t kLetterTable7Size = 48;
static const int32_t kLetterTable7[48] = {
  1073748224, 6765, 1073748592, 6873, 1073748736, 6918, 1073748755, 6935,  // NOLINT
  6941, 1073748767, 6952, 1073748778, 6966, 1073748792, 6972, 6974,  // NOLINT
  1073748800, 6977, 1073748803, 6980, 1073748806, 7089, 1073748947, 7485,  // NOLINT
  1073749328, 7567, 1073749394, 7623, 1073749488, 7675, 1073749616, 7796,  // NOLINT
  1073749622, 7932, 1073749793, 7994, 1073749825, 8026, 1073749862, 8126,  // NOLINT
  1073749954, 8135, 1073749962, 8143, 1073749970, 8151, 1073749978, 8156 };  // NOLINT
bool Letter::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kLetterTable0,
                                       kLetterTable0Size,
                                       c);
    case 1: return LookupPredicate(kLetterTable1,
                                       kLetterTable1Size,
                                       c);
    case 2: return LookupPredicate(kLetterTable2,
                                       kLetterTable2Size,
                                       c);
    case 3: return LookupPredicate(kLetterTable3,
                                       kLetterTable3Size,
                                       c);
    case 4: return LookupPredicate(kLetterTable4,
                                       kLetterTable4Size,
                                       c);
    case 5: return LookupPredicate(kLetterTable5,
                                       kLetterTable5Size,
                                       c);
    case 6: return LookupPredicate(kLetterTable6,
                                       kLetterTable6Size,
                                       c);
    case 7: return LookupPredicate(kLetterTable7,
                                       kLetterTable7Size,
                                       c);
    default: return false;
  }
}

// Space:                point.category == 'Zs'

static const uint16_t kSpaceTable0Size = 4;
static const int32_t kSpaceTable0[4] = {
  32, 160, 5760, 6158 };  // NOLINT
static const uint16_t kSpaceTable1Size = 5;
static const int32_t kSpaceTable1[5] = {
  1073741824, 10, 47, 95, 4096 };  // NOLINT
bool Space::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kSpaceTable0,
                                       kSpaceTable0Size,
                                       c);
    case 1: return LookupPredicate(kSpaceTable1,
                                       kSpaceTable1Size,
                                       c);
    default: return false;
  }
}

// Number:               point.category == 'Nd'

static const uint16_t kNumberTable0Size = 56;
static const int32_t kNumberTable0[56] = {
  1073741872, 57, 1073743456, 1641, 1073743600, 1785, 1073743808, 1993,  // NOLINT
  1073744230, 2415, 1073744358, 2543, 1073744486, 2671, 1073744614, 2799,  // NOLINT
  1073744742, 2927, 1073744870, 3055, 1073744998, 3183, 1073745126, 3311,  // NOLINT
  1073745254, 3439, 1073745488, 3673, 1073745616, 3801, 1073745696, 3881,  // NOLINT
  1073745984, 4169, 1073746064, 4249, 1073747936, 6121, 1073747984, 6169,  // NOLINT
  1073748294, 6479, 1073748432, 6617, 1073748608, 6793, 1073748624, 6809,  // NOLINT
  1073748816, 7001, 1073748912, 7097, 1073749056, 7241, 1073749072, 7257 };  // NOLINT
static const uint16_t kNumberTable5Size = 12;
static const int32_t kNumberTable5[12] = {
  1073743392, 1577, 1073744080, 2265, 1073744128, 2313, 1073744336, 2521,  // NOLINT
  1073744464, 2649, 1073744880, 3065 };  // NOLINT
static const uint16_t kNumberTable7Size = 2;
static const int32_t kNumberTable7[2] = {
  1073749776, 7961 };  // NOLINT
bool Number::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kNumberTable0,
                                       kNumberTable0Size,
                                       c);
    case 5: return LookupPredicate(kNumberTable5,
                                       kNumberTable5Size,
                                       c);
    case 7: return LookupPredicate(kNumberTable7,
                                       kNumberTable7Size,
                                       c);
    default: return false;
  }
}

// WhiteSpace:           'Ws' in point.properties

static const uint16_t kWhiteSpaceTable0Size = 7;
static const int32_t kWhiteSpaceTable0[7] = {
  1073741833, 13, 32, 133, 160, 5760, 6158 };  // NOLINT
static const uint16_t kWhiteSpaceTable1Size = 7;
static const int32_t kWhiteSpaceTable1[7] = {
  1073741824, 10, 1073741864, 41, 47, 95, 4096 };  // NOLINT
bool WhiteSpace::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kWhiteSpaceTable0,
                                       kWhiteSpaceTable0Size,
                                       c);
    case 1: return LookupPredicate(kWhiteSpaceTable1,
                                       kWhiteSpaceTable1Size,
                                       c);
    default: return false;
  }
}

// LineTerminator:       'Lt' in point.properties

static const uint16_t kLineTerminatorTable0Size = 2;
static const int32_t kLineTerminatorTable0[2] = {
  10, 13 };  // NOLINT
static const uint16_t kLineTerminatorTable1Size = 2;
static const int32_t kLineTerminatorTable1[2] = {
  1073741864, 41 };  // NOLINT
bool LineTerminator::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kLineTerminatorTable0,
                                       kLineTerminatorTable0Size,
                                       c);
    case 1: return LookupPredicate(kLineTerminatorTable1,
                                       kLineTerminatorTable1Size,
                                       c);
    default: return false;
  }
}

// CombiningMark:        point.category in ['Mn', 'Mc']

static const uint16_t kCombiningMarkTable0Size = 258;
static const int32_t kCombiningMarkTable0[258] = {
  1073742592, 879, 1073742979, 1159, 1073743249, 1469, 1471, 1073743297,  // NOLINT
  1474, 1073743300, 1477, 1479, 1073743376, 1562, 1073743435, 1631,  // NOLINT
  1648, 1073743574, 1756, 1073743583, 1764, 1073743591, 1768, 1073743594,  // NOLINT
  1773, 1809, 1073743664, 1866, 1073743782, 1968, 1073743851, 2035,  // NOLINT
  1073743894, 2073, 1073743899, 2083, 1073743909, 2087, 1073743913, 2093,  // NOLINT
  1073743961, 2139, 1073744100, 2302, 1073744128, 2307, 1073744186, 2364,  // NOLINT
  1073744190, 2383, 1073744209, 2391, 1073744226, 2403, 1073744257, 2435,  // NOLINT
  2492, 1073744318, 2500, 1073744327, 2504, 1073744331, 2509, 2519,  // NOLINT
  1073744354, 2531, 1073744385, 2563, 2620, 1073744446, 2626, 1073744455,  // NOLINT
  2632, 1073744459, 2637, 2641, 1073744496, 2673, 2677, 1073744513,  // NOLINT
  2691, 2748, 1073744574, 2757, 1073744583, 2761, 1073744587, 2765,  // NOLINT
  1073744610, 2787, 1073744641, 2819, 2876, 1073744702, 2884, 1073744711,  // NOLINT
  2888, 1073744715, 2893, 1073744726, 2903, 1073744738, 2915, 2946,  // NOLINT
  1073744830, 3010, 1073744838, 3016, 1073744842, 3021, 3031, 1073744897,  // NOLINT
  3075, 1073744958, 3140, 1073744966, 3144, 1073744970, 3149, 1073744981,  // NOLINT
  3158, 1073744994, 3171, 1073745026, 3203, 3260, 1073745086, 3268,  // NOLINT
  1073745094, 3272, 1073745098, 3277, 1073745109, 3286, 1073745122, 3299,  // NOLINT
  1073745154, 3331, 1073745214, 3396, 1073745222, 3400, 1073745226, 3405,  // NOLINT
  3415, 1073745250, 3427, 1073745282, 3459, 3530, 1073745359, 3540,  // NOLINT
  3542, 1073745368, 3551, 1073745394, 3571, 3633, 1073745460, 3642,  // NOLINT
  1073745479, 3662, 3761, 1073745588, 3769, 1073745595, 3772, 1073745608,  // NOLINT
  3789, 1073745688, 3865, 3893, 3895, 3897, 1073745726, 3903,  // NOLINT
  1073745777, 3972, 1073745798, 3975, 1073745805, 3991, 1073745817, 4028,  // NOLINT
  4038, 1073745963, 4158, 1073746006, 4185, 1073746014, 4192, 1073746018,  // NOLINT
  4196, 1073746023, 4205, 1073746033, 4212, 1073746050, 4237, 4239,  // NOLINT
  1073746074, 4253, 1073746781, 4959, 1073747730, 5908, 1073747762, 5940,  // NOLINT
  1073747794, 5971, 1073747826, 6003, 1073747892, 6099, 6109, 1073747979,  // NOLINT
  6157, 6313, 1073748256, 6443, 1073748272, 6459, 1073748400, 6592,  // NOLINT
  1073748424, 6601, 1073748503, 6683, 1073748565, 6750, 1073748576, 6780,  // NOLINT
  6783, 1073748736, 6916, 1073748788, 6980, 1073748843, 7027, 1073748864,  // NOLINT
  7042, 1073748897, 7085, 1073748966, 7155, 1073749028, 7223, 1073749200,  // NOLINT
  7378, 1073749204, 7400, 7405, 1073749234, 7412, 1073749440, 7654,  // NOLINT
  1073749500, 7679 };  // NOLINT
static const uint16_t kCombiningMarkTable1Size = 14;
static const int32_t kCombiningMarkTable1[14] = {
  1073742032, 220, 225, 1073742053, 240, 1073745135, 3313, 3455,  // NOLINT
  1073745376, 3583, 1073745962, 4143, 1073746073, 4250 };  // NOLINT
static const uint16_t kCombiningMarkTable5Size = 47;
static const int32_t kCombiningMarkTable5[47] = {
  1647, 1073743476, 1661, 1695, 1073743600, 1777, 2050, 2054,  // NOLINT
  2059, 1073743907, 2087, 1073744000, 2177, 1073744052, 2244, 1073744096,  // NOLINT
  2289, 1073744166, 2349, 1073744199, 2387, 1073744256, 2435, 1073744307,  // NOLINT
  2496, 1073744425, 2614, 2627, 1073744460, 2637, 2683, 2736,  // NOLINT
  1073744562, 2740, 1073744567, 2744, 1073744574, 2751, 2753, 1073744619,  // NOLINT
  2799, 1073744629, 2806, 1073744867, 3050, 1073744876, 3053 };  // NOLINT
static const uint16_t kCombiningMarkTable7Size = 5;
static const int32_t kCombiningMarkTable7[5] = {
  6942, 1073749504, 7695, 1073749536, 7718 };  // NOLINT
bool CombiningMark::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kCombiningMarkTable0,
                                       kCombiningMarkTable0Size,
                                       c);
    case 1: return LookupPredicate(kCombiningMarkTable1,
                                       kCombiningMarkTable1Size,
                                       c);
    case 5: return LookupPredicate(kCombiningMarkTable5,
                                       kCombiningMarkTable5Size,
                                       c);
    case 7: return LookupPredicate(kCombiningMarkTable7,
                                       kCombiningMarkTable7Size,
                                       c);
    default: return false;
  }
}

// ConnectorPunctuation: point.category == 'Pc'

static const uint16_t kConnectorPunctuationTable0Size = 1;
static const int32_t kConnectorPunctuationTable0[1] = {
  95 };  // NOLINT
static const uint16_t kConnectorPunctuationTable1Size = 3;
static const int32_t kConnectorPunctuationTable1[3] = {
  1073741887, 64, 84 };  // NOLINT
static const uint16_t kConnectorPunctuationTable7Size = 5;
static const int32_t kConnectorPunctuationTable7[5] = {
  1073749555, 7732, 1073749581, 7759, 7999 };  // NOLINT
bool ConnectorPunctuation::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kConnectorPunctuationTable0,
                                       kConnectorPunctuationTable0Size,
                                       c);
    case 1: return LookupPredicate(kConnectorPunctuationTable1,
                                       kConnectorPunctuationTable1Size,
                                       c);
    case 7: return LookupPredicate(kConnectorPunctuationTable7,
                                       kConnectorPunctuationTable7Size,
                                       c);
    default: return false;
  }
}

static const MultiCharacterSpecialCase<2> kToLowercaseMultiStrings0[2] = {  // NOLINT
  {{105, 775}}, {{kSentinel}} }; // NOLINT
static const uint16_t kToLowercaseTable0Size = 483;  // NOLINT
static const int32_t kToLowercaseTable0[966] = {
  1073741889, 128, 90, 128, 1073742016, 128, 214, 128, 1073742040, 128, 222, 128, 256, 4, 258, 4,  // NOLINT
  260, 4, 262, 4, 264, 4, 266, 4, 268, 4, 270, 4, 272, 4, 274, 4,  // NOLINT
  276, 4, 278, 4, 280, 4, 282, 4, 284, 4, 286, 4, 288, 4, 290, 4,  // NOLINT
  292, 4, 294, 4, 296, 4, 298, 4, 300, 4, 302, 4, 304, 1, 306, 4,  // NOLINT
  308, 4, 310, 4, 313, 4, 315, 4, 317, 4, 319, 4, 321, 4, 323, 4,  // NOLINT
  325, 4, 327, 4, 330, 4, 332, 4, 334, 4, 336, 4, 338, 4, 340, 4,  // NOLINT
  342, 4, 344, 4, 346, 4, 348, 4, 350, 4, 352, 4, 354, 4, 356, 4,  // NOLINT
  358, 4, 360, 4, 362, 4, 364, 4, 366, 4, 368, 4, 370, 4, 372, 4,  // NOLINT
  374, 4, 376, -484, 377, 4, 379, 4, 381, 4, 385, 840, 386, 4, 388, 4,  // NOLINT
  390, 824, 391, 4, 1073742217, 820, 394, 820, 395, 4, 398, 316, 399, 808, 400, 812,  // NOLINT
  401, 4, 403, 820, 404, 828, 406, 844, 407, 836, 408, 4, 412, 844, 413, 852,  // NOLINT
  415, 856, 416, 4, 418, 4, 420, 4, 422, 872, 423, 4, 425, 872, 428, 4,  // NOLINT
  430, 872, 431, 4, 1073742257, 868, 434, 868, 435, 4, 437, 4, 439, 876, 440, 4,  // NOLINT
  444, 4, 452, 8, 453, 4, 455, 8, 456, 4, 458, 8, 459, 4, 461, 4,  // NOLINT
  463, 4, 465, 4, 467, 4, 469, 4, 471, 4, 473, 4, 475, 4, 478, 4,  // NOLINT
  480, 4, 482, 4, 484, 4, 486, 4, 488, 4, 490, 4, 492, 4, 494, 4,  // NOLINT
  497, 8, 498, 4, 500, 4, 502, -388, 503, -224, 504, 4, 506, 4, 508, 4,  // NOLINT
  510, 4, 512, 4, 514, 4, 516, 4, 518, 4, 520, 4, 522, 4, 524, 4,  // NOLINT
  526, 4, 528, 4, 530, 4, 532, 4, 534, 4, 536, 4, 538, 4, 540, 4,  // NOLINT
  542, 4, 544, -520, 546, 4, 548, 4, 550, 4, 552, 4, 554, 4, 556, 4,  // NOLINT
  558, 4, 560, 4, 562, 4, 570, 43180, 571, 4, 573, -652, 574, 43168, 577, 4,  // NOLINT
  579, -780, 580, 276, 581, 284, 582, 4, 584, 4, 586, 4, 588, 4, 590, 4,  // NOLINT
  880, 4, 882, 4, 886, 4, 902, 152, 1073742728, 148, 906, 148, 908, 256, 1073742734, 252,  // NOLINT
  911, 252, 1073742737, 128, 929, 128, 931, 6, 1073742756, 128, 939, 128, 975, 32, 984, 4,  // NOLINT
  986, 4, 988, 4, 990, 4, 992, 4, 994, 4, 996, 4, 998, 4, 1000, 4,  // NOLINT
  1002, 4, 1004, 4, 1006, 4, 1012, -240, 1015, 4, 1017, -28, 1018, 4, 1073742845, -520,  // NOLINT
  1023, -520, 1073742848, 320, 1039, 320, 1073742864, 128, 1071, 128, 1120, 4, 1122, 4, 1124, 4,  // NOLINT
  1126, 4, 1128, 4, 1130, 4, 1132, 4, 1134, 4, 1136, 4, 1138, 4, 1140, 4,  // NOLINT
  1142, 4, 1144, 4, 1146, 4, 1148, 4, 1150, 4, 1152, 4, 1162, 4, 1164, 4,  // NOLINT
  1166, 4, 1168, 4, 1170, 4, 1172, 4, 1174, 4, 1176, 4, 1178, 4, 1180, 4,  // NOLINT
  1182, 4, 1184, 4, 1186, 4, 1188, 4, 1190, 4, 1192, 4, 1194, 4, 1196, 4,  // NOLINT
  1198, 4, 1200, 4, 1202, 4, 1204, 4, 1206, 4, 1208, 4, 1210, 4, 1212, 4,  // NOLINT
  1214, 4, 1216, 60, 1217, 4, 1219, 4, 1221, 4, 1223, 4, 1225, 4, 1227, 4,  // NOLINT
  1229, 4, 1232, 4, 1234, 4, 1236, 4, 1238, 4, 1240, 4, 1242, 4, 1244, 4,  // NOLINT
  1246, 4, 1248, 4, 1250, 4, 1252, 4, 1254, 4, 1256, 4, 1258, 4, 1260, 4,  // NOLINT
  1262, 4, 1264, 4, 1266, 4, 1268, 4, 1270, 4, 1272, 4, 1274, 4, 1276, 4,  // NOLINT
  1278, 4, 1280, 4, 1282, 4, 1284, 4, 1286, 4, 1288, 4, 1290, 4, 1292, 4,  // NOLINT
  1294, 4, 1296, 4, 1298, 4, 1300, 4, 1302, 4, 1304, 4, 1306, 4, 1308, 4,  // NOLINT
  1310, 4, 1312, 4, 1314, 4, 1316, 4, 1318, 4, 1073743153, 192, 1366, 192, 1073746080, 29056,  // NOLINT
  4293, 29056, 4295, 29056, 4301, 29056, 7680, 4, 7682, 4, 7684, 4, 7686, 4, 7688, 4,  // NOLINT
  7690, 4, 7692, 4, 7694, 4, 7696, 4, 7698, 4, 7700, 4, 7702, 4, 7704, 4,  // NOLINT
  7706, 4, 7708, 4, 7710, 4, 7712, 4, 7714, 4, 7716, 4, 7718, 4, 7720, 4,  // NOLINT
  7722, 4, 7724, 4, 7726, 4, 7728, 4, 7730, 4, 7732, 4, 7734, 4, 7736, 4,  // NOLINT
  7738, 4, 7740, 4, 7742, 4, 7744, 4, 7746, 4, 7748, 4, 7750, 4, 7752, 4,  // NOLINT
  7754, 4, 7756, 4, 7758, 4, 7760, 4, 7762, 4, 7764, 4, 7766, 4, 7768, 4,  // NOLINT
  7770, 4, 7772, 4, 7774, 4, 7776, 4, 7778, 4, 7780, 4, 7782, 4, 7784, 4,  // NOLINT
  7786, 4, 7788, 4, 7790, 4, 7792, 4, 7794, 4, 7796, 4, 7798, 4, 7800, 4,  // NOLINT
  7802, 4, 7804, 4, 7806, 4, 7808, 4, 7810, 4, 7812, 4, 7814, 4, 7816, 4,  // NOLINT
  7818, 4, 7820, 4, 7822, 4, 7824, 4, 7826, 4, 7828, 4, 7838, -30460, 7840, 4,  // NOLINT
  7842, 4, 7844, 4, 7846, 4, 7848, 4, 7850, 4, 7852, 4, 7854, 4, 7856, 4,  // NOLINT
  7858, 4, 7860, 4, 7862, 4, 7864, 4, 7866, 4, 7868, 4, 7870, 4, 7872, 4,  // NOLINT
  7874, 4, 7876, 4, 7878, 4, 7880, 4, 7882, 4, 7884, 4, 7886, 4, 7888, 4,  // NOLINT
  7890, 4, 7892, 4, 7894, 4, 7896, 4, 7898, 4, 7900, 4, 7902, 4, 7904, 4,  // NOLINT
  7906, 4, 7908, 4, 7910, 4, 7912, 4, 7914, 4, 7916, 4, 7918, 4, 7920, 4,  // NOLINT
  7922, 4, 7924, 4, 7926, 4, 7928, 4, 7930, 4, 7932, 4, 7934, 4, 1073749768, -32,  // NOLINT
  7951, -32, 1073749784, -32, 7965, -32, 1073749800, -32, 7983, -32, 1073749816, -32, 7999, -32, 1073749832, -32,  // NOLINT
  8013, -32, 8025, -32, 8027, -32, 8029, -32, 8031, -32, 1073749864, -32, 8047, -32, 1073749896, -32,  // NOLINT
  8079, -32, 1073749912, -32, 8095, -32, 1073749928, -32, 8111, -32, 1073749944, -32, 8121, -32, 1073749946, -296,  // NOLINT
  8123, -296, 8124, -36, 1073749960, -344, 8139, -344, 8140, -36, 1073749976, -32, 8153, -32, 1073749978, -400,  // NOLINT
  8155, -400, 1073749992, -32, 8169, -32, 1073749994, -448, 8171, -448, 8172, -28, 1073750008, -512, 8185, -512,  // NOLINT
  1073750010, -504, 8187, -504, 8188, -36 };  // NOLINT
static const uint16_t kToLowercaseMultiStrings0Size = 2;  // NOLINT
static const MultiCharacterSpecialCase<1> kToLowercaseMultiStrings1[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kToLowercaseTable1Size = 79;  // NOLINT
static const int32_t kToLowercaseTable1[158] = {
  294, -30068, 298, -33532, 299, -33048, 306, 112, 1073742176, 64, 367, 64, 387, 4, 1073743030, 104,  // NOLINT
  1231, 104, 1073744896, 192, 3118, 192, 3168, 4, 3170, -42972, 3171, -15256, 3172, -42908, 3175, 4,  // NOLINT
  3177, 4, 3179, 4, 3181, -43120, 3182, -42996, 3183, -43132, 3184, -43128, 3186, 4, 3189, 4,  // NOLINT
  1073745022, -43260, 3199, -43260, 3200, 4, 3202, 4, 3204, 4, 3206, 4, 3208, 4, 3210, 4,  // NOLINT
  3212, 4, 3214, 4, 3216, 4, 3218, 4, 3220, 4, 3222, 4, 3224, 4, 3226, 4,  // NOLINT
  3228, 4, 3230, 4, 3232, 4, 3234, 4, 3236, 4, 3238, 4, 3240, 4, 3242, 4,  // NOLINT
  3244, 4, 3246, 4, 3248, 4, 3250, 4, 3252, 4, 3254, 4, 3256, 4, 3258, 4,  // NOLINT
  3260, 4, 3262, 4, 3264, 4, 3266, 4, 3268, 4, 3270, 4, 3272, 4, 3274, 4,  // NOLINT
  3276, 4, 3278, 4, 3280, 4, 3282, 4, 3284, 4, 3286, 4, 3288, 4, 3290, 4,  // NOLINT
  3292, 4, 3294, 4, 3296, 4, 3298, 4, 3307, 4, 3309, 4, 3314, 4 };  // NOLINT
static const uint16_t kToLowercaseMultiStrings1Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kToLowercaseMultiStrings5[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kToLowercaseTable5Size = 91;  // NOLINT
static const int32_t kToLowercaseTable5[182] = {
  1600, 4, 1602, 4, 1604, 4, 1606, 4, 1608, 4, 1610, 4, 1612, 4, 1614, 4,  // NOLINT
  1616, 4, 1618, 4, 1620, 4, 1622, 4, 1624, 4, 1626, 4, 1628, 4, 1630, 4,  // NOLINT
  1632, 4, 1634, 4, 1636, 4, 1638, 4, 1640, 4, 1642, 4, 1644, 4, 1664, 4,  // NOLINT
  1666, 4, 1668, 4, 1670, 4, 1672, 4, 1674, 4, 1676, 4, 1678, 4, 1680, 4,  // NOLINT
  1682, 4, 1684, 4, 1686, 4, 1826, 4, 1828, 4, 1830, 4, 1832, 4, 1834, 4,  // NOLINT
  1836, 4, 1838, 4, 1842, 4, 1844, 4, 1846, 4, 1848, 4, 1850, 4, 1852, 4,  // NOLINT
  1854, 4, 1856, 4, 1858, 4, 1860, 4, 1862, 4, 1864, 4, 1866, 4, 1868, 4,  // NOLINT
  1870, 4, 1872, 4, 1874, 4, 1876, 4, 1878, 4, 1880, 4, 1882, 4, 1884, 4,  // NOLINT
  1886, 4, 1888, 4, 1890, 4, 1892, 4, 1894, 4, 1896, 4, 1898, 4, 1900, 4,  // NOLINT
  1902, 4, 1913, 4, 1915, 4, 1917, -141328, 1918, 4, 1920, 4, 1922, 4, 1924, 4,  // NOLINT
  1926, 4, 1931, 4, 1933, -169120, 1936, 4, 1938, 4, 1952, 4, 1954, 4, 1956, 4,  // NOLINT
  1958, 4, 1960, 4, 1962, -169232 };  // NOLINT
static const uint16_t kToLowercaseMultiStrings5Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kToLowercaseMultiStrings7[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kToLowercaseTable7Size = 2;  // NOLINT
static const int32_t kToLowercaseTable7[4] = {
  1073749793, 128, 7994, 128 };  // NOLINT
static const uint16_t kToLowercaseMultiStrings7Size = 1;  // NOLINT
int ToLowercase::Convert(uchar c,
                      uchar n,
                      uchar* result,
                      bool* allow_caching_ptr) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupMapping<true>(kToLowercaseTable0,
                                           kToLowercaseTable0Size,
                                           kToLowercaseMultiStrings0,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 1: return LookupMapping<true>(kToLowercaseTable1,
                                           kToLowercaseTable1Size,
                                           kToLowercaseMultiStrings1,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 5: return LookupMapping<true>(kToLowercaseTable5,
                                           kToLowercaseTable5Size,
                                           kToLowercaseMultiStrings5,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 7: return LookupMapping<true>(kToLowercaseTable7,
                                           kToLowercaseTable7Size,
                                           kToLowercaseMultiStrings7,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    default: return 0;
  }
}

static const MultiCharacterSpecialCase<3> kToUppercaseMultiStrings0[62] = {  // NOLINT
  {{83, 83, kSentinel}}, {{700, 78, kSentinel}}, {{74, 780, kSentinel}}, {{921, 776, 769}},  // NOLINT
  {{933, 776, 769}}, {{1333, 1362, kSentinel}}, {{72, 817, kSentinel}}, {{84, 776, kSentinel}},  // NOLINT
  {{87, 778, kSentinel}}, {{89, 778, kSentinel}}, {{65, 702, kSentinel}}, {{933, 787, kSentinel}},  // NOLINT
  {{933, 787, 768}}, {{933, 787, 769}}, {{933, 787, 834}}, {{7944, 921, kSentinel}},  // NOLINT
  {{7945, 921, kSentinel}}, {{7946, 921, kSentinel}}, {{7947, 921, kSentinel}}, {{7948, 921, kSentinel}},  // NOLINT
  {{7949, 921, kSentinel}}, {{7950, 921, kSentinel}}, {{7951, 921, kSentinel}}, {{7976, 921, kSentinel}},  // NOLINT
  {{7977, 921, kSentinel}}, {{7978, 921, kSentinel}}, {{7979, 921, kSentinel}}, {{7980, 921, kSentinel}},  // NOLINT
  {{7981, 921, kSentinel}}, {{7982, 921, kSentinel}}, {{7983, 921, kSentinel}}, {{8040, 921, kSentinel}},  // NOLINT
  {{8041, 921, kSentinel}}, {{8042, 921, kSentinel}}, {{8043, 921, kSentinel}}, {{8044, 921, kSentinel}},  // NOLINT
  {{8045, 921, kSentinel}}, {{8046, 921, kSentinel}}, {{8047, 921, kSentinel}}, {{8122, 921, kSentinel}},  // NOLINT
  {{913, 921, kSentinel}}, {{902, 921, kSentinel}}, {{913, 834, kSentinel}}, {{913, 834, 921}},  // NOLINT
  {{8138, 921, kSentinel}}, {{919, 921, kSentinel}}, {{905, 921, kSentinel}}, {{919, 834, kSentinel}},  // NOLINT
  {{919, 834, 921}}, {{921, 776, 768}}, {{921, 834, kSentinel}}, {{921, 776, 834}},  // NOLINT
  {{933, 776, 768}}, {{929, 787, kSentinel}}, {{933, 834, kSentinel}}, {{933, 776, 834}},  // NOLINT
  {{8186, 921, kSentinel}}, {{937, 921, kSentinel}}, {{911, 921, kSentinel}}, {{937, 834, kSentinel}},  // NOLINT
  {{937, 834, 921}}, {{kSentinel}} }; // NOLINT
static const uint16_t kToUppercaseTable0Size = 580;  // NOLINT
static const int32_t kToUppercaseTable0[1160] = {
  1073741921, -128, 122, -128, 181, 2972, 223, 1, 1073742048, -128, 246, -128, 1073742072, -128, 254, -128,  // NOLINT
  255, 484, 257, -4, 259, -4, 261, -4, 263, -4, 265, -4, 267, -4, 269, -4,  // NOLINT
  271, -4, 273, -4, 275, -4, 277, -4, 279, -4, 281, -4, 283, -4, 285, -4,  // NOLINT
  287, -4, 289, -4, 291, -4, 293, -4, 295, -4, 297, -4, 299, -4, 301, -4,  // NOLINT
  303, -4, 305, -928, 307, -4, 309, -4, 311, -4, 314, -4, 316, -4, 318, -4,  // NOLINT
  320, -4, 322, -4, 324, -4, 326, -4, 328, -4, 329, 5, 331, -4, 333, -4,  // NOLINT
  335, -4, 337, -4, 339, -4, 341, -4, 343, -4, 345, -4, 347, -4, 349, -4,  // NOLINT
  351, -4, 353, -4, 355, -4, 357, -4, 359, -4, 361, -4, 363, -4, 365, -4,  // NOLINT
  367, -4, 369, -4, 371, -4, 373, -4, 375, -4, 378, -4, 380, -4, 382, -4,  // NOLINT
  383, -1200, 384, 780, 387, -4, 389, -4, 392, -4, 396, -4, 402, -4, 405, 388,  // NOLINT
  409, -4, 410, 652, 414, 520, 417, -4, 419, -4, 421, -4, 424, -4, 429, -4,  // NOLINT
  432, -4, 436, -4, 438, -4, 441, -4, 445, -4, 447, 224, 453, -4, 454, -8,  // NOLINT
  456, -4, 457, -8, 459, -4, 460, -8, 462, -4, 464, -4, 466, -4, 468, -4,  // NOLINT
  470, -4, 472, -4, 474, -4, 476, -4, 477, -316, 479, -4, 481, -4, 483, -4,  // NOLINT
  485, -4, 487, -4, 489, -4, 491, -4, 493, -4, 495, -4, 496, 9, 498, -4,  // NOLINT
  499, -8, 501, -4, 505, -4, 507, -4, 509, -4, 511, -4, 513, -4, 515, -4,  // NOLINT
  517, -4, 519, -4, 521, -4, 523, -4, 525, -4, 527, -4, 529, -4, 531, -4,  // NOLINT
  533, -4, 535, -4, 537, -4, 539, -4, 541, -4, 543, -4, 547, -4, 549, -4,  // NOLINT
  551, -4, 553, -4, 555, -4, 557, -4, 559, -4, 561, -4, 563, -4, 572, -4,  // NOLINT
  1073742399, 43260, 576, 43260, 578, -4, 583, -4, 585, -4, 587, -4, 589, -4, 591, -4,  // NOLINT
  592, 43132, 593, 43120, 594, 43128, 595, -840, 596, -824, 1073742422, -820, 599, -820, 601, -808,  // NOLINT
  603, -812, 608, -820, 611, -828, 613, 169120, 614, 169232, 616, -836, 617, -844, 619, 42972,  // NOLINT
  623, -844, 625, 42996, 626, -852, 629, -856, 637, 42908, 640, -872, 643, -872, 648, -872,  // NOLINT
  649, -276, 1073742474, -868, 651, -868, 652, -284, 658, -876, 837, 336, 881, -4, 883, -4,  // NOLINT
  887, -4, 1073742715, 520, 893, 520, 912, 13, 940, -152, 1073742765, -148, 943, -148, 944, 17,  // NOLINT
  1073742769, -128, 961, -128, 962, -124, 1073742787, -128, 971, -128, 972, -256, 1073742797, -252, 974, -252,  // NOLINT
  976, -248, 977, -228, 981, -188, 982, -216, 983, -32, 985, -4, 987, -4, 989, -4,  // NOLINT
  991, -4, 993, -4, 995, -4, 997, -4, 999, -4, 1001, -4, 1003, -4, 1005, -4,  // NOLINT
  1007, -4, 1008, -344, 1009, -320, 1010, 28, 1013, -384, 1016, -4, 1019, -4, 1073742896, -128,  // NOLINT
  1103, -128, 1073742928, -320, 1119, -320, 1121, -4, 1123, -4, 1125, -4, 1127, -4, 1129, -4,  // NOLINT
  1131, -4, 1133, -4, 1135, -4, 1137, -4, 1139, -4, 1141, -4, 1143, -4, 1145, -4,  // NOLINT
  1147, -4, 1149, -4, 1151, -4, 1153, -4, 1163, -4, 1165, -4, 1167, -4, 1169, -4,  // NOLINT
  1171, -4, 1173, -4, 1175, -4, 1177, -4, 1179, -4, 1181, -4, 1183, -4, 1185, -4,  // NOLINT
  1187, -4, 1189, -4, 1191, -4, 1193, -4, 1195, -4, 1197, -4, 1199, -4, 1201, -4,  // NOLINT
  1203, -4, 1205, -4, 1207, -4, 1209, -4, 1211, -4, 1213, -4, 1215, -4, 1218, -4,  // NOLINT
  1220, -4, 1222, -4, 1224, -4, 1226, -4, 1228, -4, 1230, -4, 1231, -60, 1233, -4,  // NOLINT
  1235, -4, 1237, -4, 1239, -4, 1241, -4, 1243, -4, 1245, -4, 1247, -4, 1249, -4,  // NOLINT
  1251, -4, 1253, -4, 1255, -4, 1257, -4, 1259, -4, 1261, -4, 1263, -4, 1265, -4,  // NOLINT
  1267, -4, 1269, -4, 1271, -4, 1273, -4, 1275, -4, 1277, -4, 1279, -4, 1281, -4,  // NOLINT
  1283, -4, 1285, -4, 1287, -4, 1289, -4, 1291, -4, 1293, -4, 1295, -4, 1297, -4,  // NOLINT
  1299, -4, 1301, -4, 1303, -4, 1305, -4, 1307, -4, 1309, -4, 1311, -4, 1313, -4,  // NOLINT
  1315, -4, 1317, -4, 1319, -4, 1073743201, -192, 1414, -192, 1415, 21, 7545, 141328, 7549, 15256,  // NOLINT
  7681, -4, 7683, -4, 7685, -4, 7687, -4, 7689, -4, 7691, -4, 7693, -4, 7695, -4,  // NOLINT
  7697, -4, 7699, -4, 7701, -4, 7703, -4, 7705, -4, 7707, -4, 7709, -4, 7711, -4,  // NOLINT
  7713, -4, 7715, -4, 7717, -4, 7719, -4, 7721, -4, 7723, -4, 7725, -4, 7727, -4,  // NOLINT
  7729, -4, 7731, -4, 7733, -4, 7735, -4, 7737, -4, 7739, -4, 7741, -4, 7743, -4,  // NOLINT
  7745, -4, 7747, -4, 7749, -4, 7751, -4, 7753, -4, 7755, -4, 7757, -4, 7759, -4,  // NOLINT
  7761, -4, 7763, -4, 7765, -4, 7767, -4, 7769, -4, 7771, -4, 7773, -4, 7775, -4,  // NOLINT
  7777, -4, 7779, -4, 7781, -4, 7783, -4, 7785, -4, 7787, -4, 7789, -4, 7791, -4,  // NOLINT
  7793, -4, 7795, -4, 7797, -4, 7799, -4, 7801, -4, 7803, -4, 7805, -4, 7807, -4,  // NOLINT
  7809, -4, 7811, -4, 7813, -4, 7815, -4, 7817, -4, 7819, -4, 7821, -4, 7823, -4,  // NOLINT
  7825, -4, 7827, -4, 7829, -4, 7830, 25, 7831, 29, 7832, 33, 7833, 37, 7834, 41,  // NOLINT
  7835, -236, 7841, -4, 7843, -4, 7845, -4, 7847, -4, 7849, -4, 7851, -4, 7853, -4,  // NOLINT
  7855, -4, 7857, -4, 7859, -4, 7861, -4, 7863, -4, 7865, -4, 7867, -4, 7869, -4,  // NOLINT
  7871, -4, 7873, -4, 7875, -4, 7877, -4, 7879, -4, 7881, -4, 7883, -4, 7885, -4,  // NOLINT
  7887, -4, 7889, -4, 7891, -4, 7893, -4, 7895, -4, 7897, -4, 7899, -4, 7901, -4,  // NOLINT
  7903, -4, 7905, -4, 7907, -4, 7909, -4, 7911, -4, 7913, -4, 7915, -4, 7917, -4,  // NOLINT
  7919, -4, 7921, -4, 7923, -4, 7925, -4, 7927, -4, 7929, -4, 7931, -4, 7933, -4,  // NOLINT
  7935, -4, 1073749760, 32, 7943, 32, 1073749776, 32, 7957, 32, 1073749792, 32, 7975, 32, 1073749808, 32,  // NOLINT
  7991, 32, 1073749824, 32, 8005, 32, 8016, 45, 8017, 32, 8018, 49, 8019, 32, 8020, 53,  // NOLINT
  8021, 32, 8022, 57, 8023, 32, 1073749856, 32, 8039, 32, 1073749872, 296, 8049, 296, 1073749874, 344,  // NOLINT
  8053, 344, 1073749878, 400, 8055, 400, 1073749880, 512, 8057, 512, 1073749882, 448, 8059, 448, 1073749884, 504,  // NOLINT
  8061, 504, 8064, 61, 8065, 65, 8066, 69, 8067, 73, 8068, 77, 8069, 81, 8070, 85,  // NOLINT
  8071, 89, 8072, 61, 8073, 65, 8074, 69, 8075, 73, 8076, 77, 8077, 81, 8078, 85,  // NOLINT
  8079, 89, 8080, 93, 8081, 97, 8082, 101, 8083, 105, 8084, 109, 8085, 113, 8086, 117,  // NOLINT
  8087, 121, 8088, 93, 8089, 97, 8090, 101, 8091, 105, 8092, 109, 8093, 113, 8094, 117,  // NOLINT
  8095, 121, 8096, 125, 8097, 129, 8098, 133, 8099, 137, 8100, 141, 8101, 145, 8102, 149,  // NOLINT
  8103, 153, 8104, 125, 8105, 129, 8106, 133, 8107, 137, 8108, 141, 8109, 145, 8110, 149,  // NOLINT
  8111, 153, 1073749936, 32, 8113, 32, 8114, 157, 8115, 161, 8116, 165, 8118, 169, 8119, 173,  // NOLINT
  8124, 161, 8126, -28820, 8130, 177, 8131, 181, 8132, 185, 8134, 189, 8135, 193, 8140, 181,  // NOLINT
  1073749968, 32, 8145, 32, 8146, 197, 8147, 13, 8150, 201, 8151, 205, 1073749984, 32, 8161, 32,  // NOLINT
  8162, 209, 8163, 17, 8164, 213, 8165, 28, 8166, 217, 8167, 221, 8178, 225, 8179, 229,  // NOLINT
  8180, 233, 8182, 237, 8183, 241, 8188, 229 };  // NOLINT
static const uint16_t kToUppercaseMultiStrings0Size = 62;  // NOLINT
static const MultiCharacterSpecialCase<1> kToUppercaseMultiStrings1[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kToUppercaseTable1Size = 73;  // NOLINT
static const int32_t kToUppercaseTable1[146] = {
  334, -112, 1073742192, -64, 383, -64, 388, -4, 1073743056, -104, 1257, -104, 1073744944, -192, 3166, -192,  // NOLINT
  3169, -4, 3173, -43180, 3174, -43168, 3176, -4, 3178, -4, 3180, -4, 3187, -4, 3190, -4,  // NOLINT
  3201, -4, 3203, -4, 3205, -4, 3207, -4, 3209, -4, 3211, -4, 3213, -4, 3215, -4,  // NOLINT
  3217, -4, 3219, -4, 3221, -4, 3223, -4, 3225, -4, 3227, -4, 3229, -4, 3231, -4,  // NOLINT
  3233, -4, 3235, -4, 3237, -4, 3239, -4, 3241, -4, 3243, -4, 3245, -4, 3247, -4,  // NOLINT
  3249, -4, 3251, -4, 3253, -4, 3255, -4, 3257, -4, 3259, -4, 3261, -4, 3263, -4,  // NOLINT
  3265, -4, 3267, -4, 3269, -4, 3271, -4, 3273, -4, 3275, -4, 3277, -4, 3279, -4,  // NOLINT
  3281, -4, 3283, -4, 3285, -4, 3287, -4, 3289, -4, 3291, -4, 3293, -4, 3295, -4,  // NOLINT
  3297, -4, 3299, -4, 3308, -4, 3310, -4, 3315, -4, 1073745152, -29056, 3365, -29056, 3367, -29056,  // NOLINT
  3373, -29056 };  // NOLINT
static const uint16_t kToUppercaseMultiStrings1Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kToUppercaseMultiStrings5[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kToUppercaseTable5Size = 88;  // NOLINT
static const int32_t kToUppercaseTable5[176] = {
  1601, -4, 1603, -4, 1605, -4, 1607, -4, 1609, -4, 1611, -4, 1613, -4, 1615, -4,  // NOLINT
  1617, -4, 1619, -4, 1621, -4, 1623, -4, 1625, -4, 1627, -4, 1629, -4, 1631, -4,  // NOLINT
  1633, -4, 1635, -4, 1637, -4, 1639, -4, 1641, -4, 1643, -4, 1645, -4, 1665, -4,  // NOLINT
  1667, -4, 1669, -4, 1671, -4, 1673, -4, 1675, -4, 1677, -4, 1679, -4, 1681, -4,  // NOLINT
  1683, -4, 1685, -4, 1687, -4, 1827, -4, 1829, -4, 1831, -4, 1833, -4, 1835, -4,  // NOLINT
  1837, -4, 1839, -4, 1843, -4, 1845, -4, 1847, -4, 1849, -4, 1851, -4, 1853, -4,  // NOLINT
  1855, -4, 1857, -4, 1859, -4, 1861, -4, 1863, -4, 1865, -4, 1867, -4, 1869, -4,  // NOLINT
  1871, -4, 1873, -4, 1875, -4, 1877, -4, 1879, -4, 1881, -4, 1883, -4, 1885, -4,  // NOLINT
  1887, -4, 1889, -4, 1891, -4, 1893, -4, 1895, -4, 1897, -4, 1899, -4, 1901, -4,  // NOLINT
  1903, -4, 1914, -4, 1916, -4, 1919, -4, 1921, -4, 1923, -4, 1925, -4, 1927, -4,  // NOLINT
  1932, -4, 1937, -4, 1939, -4, 1953, -4, 1955, -4, 1957, -4, 1959, -4, 1961, -4 };  // NOLINT
static const uint16_t kToUppercaseMultiStrings5Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<3> kToUppercaseMultiStrings7[12] = {  // NOLINT
  {{70, 70, kSentinel}}, {{70, 73, kSentinel}}, {{70, 76, kSentinel}}, {{70, 70, 73}},  // NOLINT
  {{70, 70, 76}}, {{83, 84, kSentinel}}, {{1348, 1350, kSentinel}}, {{1348, 1333, kSentinel}},  // NOLINT
  {{1348, 1339, kSentinel}}, {{1358, 1350, kSentinel}}, {{1348, 1341, kSentinel}}, {{kSentinel}} }; // NOLINT
static const uint16_t kToUppercaseTable7Size = 14;  // NOLINT
static const int32_t kToUppercaseTable7[28] = {
  6912, 1, 6913, 5, 6914, 9, 6915, 13, 6916, 17, 6917, 21, 6918, 21, 6931, 25,  // NOLINT
  6932, 29, 6933, 33, 6934, 37, 6935, 41, 1073749825, -128, 8026, -128 };  // NOLINT
static const uint16_t kToUppercaseMultiStrings7Size = 12;  // NOLINT
int ToUppercase::Convert(uchar c,
                      uchar n,
                      uchar* result,
                      bool* allow_caching_ptr) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupMapping<true>(kToUppercaseTable0,
                                           kToUppercaseTable0Size,
                                           kToUppercaseMultiStrings0,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 1: return LookupMapping<true>(kToUppercaseTable1,
                                           kToUppercaseTable1Size,
                                           kToUppercaseMultiStrings1,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 5: return LookupMapping<true>(kToUppercaseTable5,
                                           kToUppercaseTable5Size,
                                           kToUppercaseMultiStrings5,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 7: return LookupMapping<true>(kToUppercaseTable7,
                                           kToUppercaseTable7Size,
                                           kToUppercaseMultiStrings7,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    default: return 0;
  }
}

static const MultiCharacterSpecialCase<1> kEcma262CanonicalizeMultiStrings0[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262CanonicalizeTable0Size = 488;  // NOLINT
static const int32_t kEcma262CanonicalizeTable0[976] = {
  1073741921, -128, 122, -128, 181, 2972, 1073742048, -128, 246, -128, 1073742072, -128, 254, -128, 255, 484,  // NOLINT
  257, -4, 259, -4, 261, -4, 263, -4, 265, -4, 267, -4, 269, -4, 271, -4,  // NOLINT
  273, -4, 275, -4, 277, -4, 279, -4, 281, -4, 283, -4, 285, -4, 287, -4,  // NOLINT
  289, -4, 291, -4, 293, -4, 295, -4, 297, -4, 299, -4, 301, -4, 303, -4,  // NOLINT
  307, -4, 309, -4, 311, -4, 314, -4, 316, -4, 318, -4, 320, -4, 322, -4,  // NOLINT
  324, -4, 326, -4, 328, -4, 331, -4, 333, -4, 335, -4, 337, -4, 339, -4,  // NOLINT
  341, -4, 343, -4, 345, -4, 347, -4, 349, -4, 351, -4, 353, -4, 355, -4,  // NOLINT
  357, -4, 359, -4, 361, -4, 363, -4, 365, -4, 367, -4, 369, -4, 371, -4,  // NOLINT
  373, -4, 375, -4, 378, -4, 380, -4, 382, -4, 384, 780, 387, -4, 389, -4,  // NOLINT
  392, -4, 396, -4, 402, -4, 405, 388, 409, -4, 410, 652, 414, 520, 417, -4,  // NOLINT
  419, -4, 421, -4, 424, -4, 429, -4, 432, -4, 436, -4, 438, -4, 441, -4,  // NOLINT
  445, -4, 447, 224, 453, -4, 454, -8, 456, -4, 457, -8, 459, -4, 460, -8,  // NOLINT
  462, -4, 464, -4, 466, -4, 468, -4, 470, -4, 472, -4, 474, -4, 476, -4,  // NOLINT
  477, -316, 479, -4, 481, -4, 483, -4, 485, -4, 487, -4, 489, -4, 491, -4,  // NOLINT
  493, -4, 495, -4, 498, -4, 499, -8, 501, -4, 505, -4, 507, -4, 509, -4,  // NOLINT
  511, -4, 513, -4, 515, -4, 517, -4, 519, -4, 521, -4, 523, -4, 525, -4,  // NOLINT
  527, -4, 529, -4, 531, -4, 533, -4, 535, -4, 537, -4, 539, -4, 541, -4,  // NOLINT
  543, -4, 547, -4, 549, -4, 551, -4, 553, -4, 555, -4, 557, -4, 559, -4,  // NOLINT
  561, -4, 563, -4, 572, -4, 1073742399, 43260, 576, 43260, 578, -4, 583, -4, 585, -4,  // NOLINT
  587, -4, 589, -4, 591, -4, 592, 43132, 593, 43120, 594, 43128, 595, -840, 596, -824,  // NOLINT
  1073742422, -820, 599, -820, 601, -808, 603, -812, 608, -820, 611, -828, 613, 169120, 614, 169232,  // NOLINT
  616, -836, 617, -844, 619, 42972, 623, -844, 625, 42996, 626, -852, 629, -856, 637, 42908,  // NOLINT
  640, -872, 643, -872, 648, -872, 649, -276, 1073742474, -868, 651, -868, 652, -284, 658, -876,  // NOLINT
  837, 336, 881, -4, 883, -4, 887, -4, 1073742715, 520, 893, 520, 940, -152, 1073742765, -148,  // NOLINT
  943, -148, 1073742769, -128, 961, -128, 962, -124, 1073742787, -128, 971, -128, 972, -256, 1073742797, -252,  // NOLINT
  974, -252, 976, -248, 977, -228, 981, -188, 982, -216, 983, -32, 985, -4, 987, -4,  // NOLINT
  989, -4, 991, -4, 993, -4, 995, -4, 997, -4, 999, -4, 1001, -4, 1003, -4,  // NOLINT
  1005, -4, 1007, -4, 1008, -344, 1009, -320, 1010, 28, 1013, -384, 1016, -4, 1019, -4,  // NOLINT
  1073742896, -128, 1103, -128, 1073742928, -320, 1119, -320, 1121, -4, 1123, -4, 1125, -4, 1127, -4,  // NOLINT
  1129, -4, 1131, -4, 1133, -4, 1135, -4, 1137, -4, 1139, -4, 1141, -4, 1143, -4,  // NOLINT
  1145, -4, 1147, -4, 1149, -4, 1151, -4, 1153, -4, 1163, -4, 1165, -4, 1167, -4,  // NOLINT
  1169, -4, 1171, -4, 1173, -4, 1175, -4, 1177, -4, 1179, -4, 1181, -4, 1183, -4,  // NOLINT
  1185, -4, 1187, -4, 1189, -4, 1191, -4, 1193, -4, 1195, -4, 1197, -4, 1199, -4,  // NOLINT
  1201, -4, 1203, -4, 1205, -4, 1207, -4, 1209, -4, 1211, -4, 1213, -4, 1215, -4,  // NOLINT
  1218, -4, 1220, -4, 1222, -4, 1224, -4, 1226, -4, 1228, -4, 1230, -4, 1231, -60,  // NOLINT
  1233, -4, 1235, -4, 1237, -4, 1239, -4, 1241, -4, 1243, -4, 1245, -4, 1247, -4,  // NOLINT
  1249, -4, 1251, -4, 1253, -4, 1255, -4, 1257, -4, 1259, -4, 1261, -4, 1263, -4,  // NOLINT
  1265, -4, 1267, -4, 1269, -4, 1271, -4, 1273, -4, 1275, -4, 1277, -4, 1279, -4,  // NOLINT
  1281, -4, 1283, -4, 1285, -4, 1287, -4, 1289, -4, 1291, -4, 1293, -4, 1295, -4,  // NOLINT
  1297, -4, 1299, -4, 1301, -4, 1303, -4, 1305, -4, 1307, -4, 1309, -4, 1311, -4,  // NOLINT
  1313, -4, 1315, -4, 1317, -4, 1319, -4, 1073743201, -192, 1414, -192, 7545, 141328, 7549, 15256,  // NOLINT
  7681, -4, 7683, -4, 7685, -4, 7687, -4, 7689, -4, 7691, -4, 7693, -4, 7695, -4,  // NOLINT
  7697, -4, 7699, -4, 7701, -4, 7703, -4, 7705, -4, 7707, -4, 7709, -4, 7711, -4,  // NOLINT
  7713, -4, 7715, -4, 7717, -4, 7719, -4, 7721, -4, 7723, -4, 7725, -4, 7727, -4,  // NOLINT
  7729, -4, 7731, -4, 7733, -4, 7735, -4, 7737, -4, 7739, -4, 7741, -4, 7743, -4,  // NOLINT
  7745, -4, 7747, -4, 7749, -4, 7751, -4, 7753, -4, 7755, -4, 7757, -4, 7759, -4,  // NOLINT
  7761, -4, 7763, -4, 7765, -4, 7767, -4, 7769, -4, 7771, -4, 7773, -4, 7775, -4,  // NOLINT
  7777, -4, 7779, -4, 7781, -4, 7783, -4, 7785, -4, 7787, -4, 7789, -4, 7791, -4,  // NOLINT
  7793, -4, 7795, -4, 7797, -4, 7799, -4, 7801, -4, 7803, -4, 7805, -4, 7807, -4,  // NOLINT
  7809, -4, 7811, -4, 7813, -4, 7815, -4, 7817, -4, 7819, -4, 7821, -4, 7823, -4,  // NOLINT
  7825, -4, 7827, -4, 7829, -4, 7835, -236, 7841, -4, 7843, -4, 7845, -4, 7847, -4,  // NOLINT
  7849, -4, 7851, -4, 7853, -4, 7855, -4, 7857, -4, 7859, -4, 7861, -4, 7863, -4,  // NOLINT
  7865, -4, 7867, -4, 7869, -4, 7871, -4, 7873, -4, 7875, -4, 7877, -4, 7879, -4,  // NOLINT
  7881, -4, 7883, -4, 7885, -4, 7887, -4, 7889, -4, 7891, -4, 7893, -4, 7895, -4,  // NOLINT
  7897, -4, 7899, -4, 7901, -4, 7903, -4, 7905, -4, 7907, -4, 7909, -4, 7911, -4,  // NOLINT
  7913, -4, 7915, -4, 7917, -4, 7919, -4, 7921, -4, 7923, -4, 7925, -4, 7927, -4,  // NOLINT
  7929, -4, 7931, -4, 7933, -4, 7935, -4, 1073749760, 32, 7943, 32, 1073749776, 32, 7957, 32,  // NOLINT
  1073749792, 32, 7975, 32, 1073749808, 32, 7991, 32, 1073749824, 32, 8005, 32, 8017, 32, 8019, 32,  // NOLINT
  8021, 32, 8023, 32, 1073749856, 32, 8039, 32, 1073749872, 296, 8049, 296, 1073749874, 344, 8053, 344,  // NOLINT
  1073749878, 400, 8055, 400, 1073749880, 512, 8057, 512, 1073749882, 448, 8059, 448, 1073749884, 504, 8061, 504,  // NOLINT
  1073749936, 32, 8113, 32, 8126, -28820, 1073749968, 32, 8145, 32, 1073749984, 32, 8161, 32, 8165, 28 };  // NOLINT
static const uint16_t kEcma262CanonicalizeMultiStrings0Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kEcma262CanonicalizeMultiStrings1[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262CanonicalizeTable1Size = 73;  // NOLINT
static const int32_t kEcma262CanonicalizeTable1[146] = {
  334, -112, 1073742192, -64, 383, -64, 388, -4, 1073743056, -104, 1257, -104, 1073744944, -192, 3166, -192,  // NOLINT
  3169, -4, 3173, -43180, 3174, -43168, 3176, -4, 3178, -4, 3180, -4, 3187, -4, 3190, -4,  // NOLINT
  3201, -4, 3203, -4, 3205, -4, 3207, -4, 3209, -4, 3211, -4, 3213, -4, 3215, -4,  // NOLINT
  3217, -4, 3219, -4, 3221, -4, 3223, -4, 3225, -4, 3227, -4, 3229, -4, 3231, -4,  // NOLINT
  3233, -4, 3235, -4, 3237, -4, 3239, -4, 3241, -4, 3243, -4, 3245, -4, 3247, -4,  // NOLINT
  3249, -4, 3251, -4, 3253, -4, 3255, -4, 3257, -4, 3259, -4, 3261, -4, 3263, -4,  // NOLINT
  3265, -4, 3267, -4, 3269, -4, 3271, -4, 3273, -4, 3275, -4, 3277, -4, 3279, -4,  // NOLINT
  3281, -4, 3283, -4, 3285, -4, 3287, -4, 3289, -4, 3291, -4, 3293, -4, 3295, -4,  // NOLINT
  3297, -4, 3299, -4, 3308, -4, 3310, -4, 3315, -4, 1073745152, -29056, 3365, -29056, 3367, -29056,  // NOLINT
  3373, -29056 };  // NOLINT
static const uint16_t kEcma262CanonicalizeMultiStrings1Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kEcma262CanonicalizeMultiStrings5[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262CanonicalizeTable5Size = 88;  // NOLINT
static const int32_t kEcma262CanonicalizeTable5[176] = {
  1601, -4, 1603, -4, 1605, -4, 1607, -4, 1609, -4, 1611, -4, 1613, -4, 1615, -4,  // NOLINT
  1617, -4, 1619, -4, 1621, -4, 1623, -4, 1625, -4, 1627, -4, 1629, -4, 1631, -4,  // NOLINT
  1633, -4, 1635, -4, 1637, -4, 1639, -4, 1641, -4, 1643, -4, 1645, -4, 1665, -4,  // NOLINT
  1667, -4, 1669, -4, 1671, -4, 1673, -4, 1675, -4, 1677, -4, 1679, -4, 1681, -4,  // NOLINT
  1683, -4, 1685, -4, 1687, -4, 1827, -4, 1829, -4, 1831, -4, 1833, -4, 1835, -4,  // NOLINT
  1837, -4, 1839, -4, 1843, -4, 1845, -4, 1847, -4, 1849, -4, 1851, -4, 1853, -4,  // NOLINT
  1855, -4, 1857, -4, 1859, -4, 1861, -4, 1863, -4, 1865, -4, 1867, -4, 1869, -4,  // NOLINT
  1871, -4, 1873, -4, 1875, -4, 1877, -4, 1879, -4, 1881, -4, 1883, -4, 1885, -4,  // NOLINT
  1887, -4, 1889, -4, 1891, -4, 1893, -4, 1895, -4, 1897, -4, 1899, -4, 1901, -4,  // NOLINT
  1903, -4, 1914, -4, 1916, -4, 1919, -4, 1921, -4, 1923, -4, 1925, -4, 1927, -4,  // NOLINT
  1932, -4, 1937, -4, 1939, -4, 1953, -4, 1955, -4, 1957, -4, 1959, -4, 1961, -4 };  // NOLINT
static const uint16_t kEcma262CanonicalizeMultiStrings5Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kEcma262CanonicalizeMultiStrings7[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262CanonicalizeTable7Size = 2;  // NOLINT
static const int32_t kEcma262CanonicalizeTable7[4] = {
  1073749825, -128, 8026, -128 };  // NOLINT
static const uint16_t kEcma262CanonicalizeMultiStrings7Size = 1;  // NOLINT
int Ecma262Canonicalize::Convert(uchar c,
                      uchar n,
                      uchar* result,
                      bool* allow_caching_ptr) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupMapping<true>(kEcma262CanonicalizeTable0,
                                           kEcma262CanonicalizeTable0Size,
                                           kEcma262CanonicalizeMultiStrings0,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 1: return LookupMapping<true>(kEcma262CanonicalizeTable1,
                                           kEcma262CanonicalizeTable1Size,
                                           kEcma262CanonicalizeMultiStrings1,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 5: return LookupMapping<true>(kEcma262CanonicalizeTable5,
                                           kEcma262CanonicalizeTable5Size,
                                           kEcma262CanonicalizeMultiStrings5,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 7: return LookupMapping<true>(kEcma262CanonicalizeTable7,
                                           kEcma262CanonicalizeTable7Size,
                                           kEcma262CanonicalizeMultiStrings7,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    default: return 0;
  }
}

static const MultiCharacterSpecialCase<4> kEcma262UnCanonicalizeMultiStrings0[497] = {  // NOLINT
  {{65, 97, kSentinel}}, {{90, 122, kSentinel}}, {{181, 924, 956, kSentinel}}, {{192, 224, kSentinel}},  // NOLINT
  {{214, 246, kSentinel}}, {{216, 248, kSentinel}}, {{222, 254, kSentinel}}, {{255, 376, kSentinel}},  // NOLINT
  {{256, 257, kSentinel}}, {{258, 259, kSentinel}}, {{260, 261, kSentinel}}, {{262, 263, kSentinel}},  // NOLINT
  {{264, 265, kSentinel}}, {{266, 267, kSentinel}}, {{268, 269, kSentinel}}, {{270, 271, kSentinel}},  // NOLINT
  {{272, 273, kSentinel}}, {{274, 275, kSentinel}}, {{276, 277, kSentinel}}, {{278, 279, kSentinel}},  // NOLINT
  {{280, 281, kSentinel}}, {{282, 283, kSentinel}}, {{284, 285, kSentinel}}, {{286, 287, kSentinel}},  // NOLINT
  {{288, 289, kSentinel}}, {{290, 291, kSentinel}}, {{292, 293, kSentinel}}, {{294, 295, kSentinel}},  // NOLINT
  {{296, 297, kSentinel}}, {{298, 299, kSentinel}}, {{300, 301, kSentinel}}, {{302, 303, kSentinel}},  // NOLINT
  {{306, 307, kSentinel}}, {{308, 309, kSentinel}}, {{310, 311, kSentinel}}, {{313, 314, kSentinel}},  // NOLINT
  {{315, 316, kSentinel}}, {{317, 318, kSentinel}}, {{319, 320, kSentinel}}, {{321, 322, kSentinel}},  // NOLINT
  {{323, 324, kSentinel}}, {{325, 326, kSentinel}}, {{327, 328, kSentinel}}, {{330, 331, kSentinel}},  // NOLINT
  {{332, 333, kSentinel}}, {{334, 335, kSentinel}}, {{336, 337, kSentinel}}, {{338, 339, kSentinel}},  // NOLINT
  {{340, 341, kSentinel}}, {{342, 343, kSentinel}}, {{344, 345, kSentinel}}, {{346, 347, kSentinel}},  // NOLINT
  {{348, 349, kSentinel}}, {{350, 351, kSentinel}}, {{352, 353, kSentinel}}, {{354, 355, kSentinel}},  // NOLINT
  {{356, 357, kSentinel}}, {{358, 359, kSentinel}}, {{360, 361, kSentinel}}, {{362, 363, kSentinel}},  // NOLINT
  {{364, 365, kSentinel}}, {{366, 367, kSentinel}}, {{368, 369, kSentinel}}, {{370, 371, kSentinel}},  // NOLINT
  {{372, 373, kSentinel}}, {{374, 375, kSentinel}}, {{377, 378, kSentinel}}, {{379, 380, kSentinel}},  // NOLINT
  {{381, 382, kSentinel}}, {{384, 579, kSentinel}}, {{385, 595, kSentinel}}, {{386, 387, kSentinel}},  // NOLINT
  {{388, 389, kSentinel}}, {{390, 596, kSentinel}}, {{391, 392, kSentinel}}, {{393, 598, kSentinel}},  // NOLINT
  {{394, 599, kSentinel}}, {{395, 396, kSentinel}}, {{398, 477, kSentinel}}, {{399, 601, kSentinel}},  // NOLINT
  {{400, 603, kSentinel}}, {{401, 402, kSentinel}}, {{403, 608, kSentinel}}, {{404, 611, kSentinel}},  // NOLINT
  {{405, 502, kSentinel}}, {{406, 617, kSentinel}}, {{407, 616, kSentinel}}, {{408, 409, kSentinel}},  // NOLINT
  {{410, 573, kSentinel}}, {{412, 623, kSentinel}}, {{413, 626, kSentinel}}, {{414, 544, kSentinel}},  // NOLINT
  {{415, 629, kSentinel}}, {{416, 417, kSentinel}}, {{418, 419, kSentinel}}, {{420, 421, kSentinel}},  // NOLINT
  {{422, 640, kSentinel}}, {{423, 424, kSentinel}}, {{425, 643, kSentinel}}, {{428, 429, kSentinel}},  // NOLINT
  {{430, 648, kSentinel}}, {{431, 432, kSentinel}}, {{433, 650, kSentinel}}, {{434, 651, kSentinel}},  // NOLINT
  {{435, 436, kSentinel}}, {{437, 438, kSentinel}}, {{439, 658, kSentinel}}, {{440, 441, kSentinel}},  // NOLINT
  {{444, 445, kSentinel}}, {{447, 503, kSentinel}}, {{452, 453, 454, kSentinel}}, {{455, 456, 457, kSentinel}},  // NOLINT
  {{458, 459, 460, kSentinel}}, {{461, 462, kSentinel}}, {{463, 464, kSentinel}}, {{465, 466, kSentinel}},  // NOLINT
  {{467, 468, kSentinel}}, {{469, 470, kSentinel}}, {{471, 472, kSentinel}}, {{473, 474, kSentinel}},  // NOLINT
  {{475, 476, kSentinel}}, {{478, 479, kSentinel}}, {{480, 481, kSentinel}}, {{482, 483, kSentinel}},  // NOLINT
  {{484, 485, kSentinel}}, {{486, 487, kSentinel}}, {{488, 489, kSentinel}}, {{490, 491, kSentinel}},  // NOLINT
  {{492, 493, kSentinel}}, {{494, 495, kSentinel}}, {{497, 498, 499, kSentinel}}, {{500, 501, kSentinel}},  // NOLINT
  {{504, 505, kSentinel}}, {{506, 507, kSentinel}}, {{508, 509, kSentinel}}, {{510, 511, kSentinel}},  // NOLINT
  {{512, 513, kSentinel}}, {{514, 515, kSentinel}}, {{516, 517, kSentinel}}, {{518, 519, kSentinel}},  // NOLINT
  {{520, 521, kSentinel}}, {{522, 523, kSentinel}}, {{524, 525, kSentinel}}, {{526, 527, kSentinel}},  // NOLINT
  {{528, 529, kSentinel}}, {{530, 531, kSentinel}}, {{532, 533, kSentinel}}, {{534, 535, kSentinel}},  // NOLINT
  {{536, 537, kSentinel}}, {{538, 539, kSentinel}}, {{540, 541, kSentinel}}, {{542, 543, kSentinel}},  // NOLINT
  {{546, 547, kSentinel}}, {{548, 549, kSentinel}}, {{550, 551, kSentinel}}, {{552, 553, kSentinel}},  // NOLINT
  {{554, 555, kSentinel}}, {{556, 557, kSentinel}}, {{558, 559, kSentinel}}, {{560, 561, kSentinel}},  // NOLINT
  {{562, 563, kSentinel}}, {{570, 11365, kSentinel}}, {{571, 572, kSentinel}}, {{574, 11366, kSentinel}},  // NOLINT
  {{575, 11390, kSentinel}}, {{576, 11391, kSentinel}}, {{577, 578, kSentinel}}, {{580, 649, kSentinel}},  // NOLINT
  {{581, 652, kSentinel}}, {{582, 583, kSentinel}}, {{584, 585, kSentinel}}, {{586, 587, kSentinel}},  // NOLINT
  {{588, 589, kSentinel}}, {{590, 591, kSentinel}}, {{592, 11375, kSentinel}}, {{593, 11373, kSentinel}},  // NOLINT
  {{594, 11376, kSentinel}}, {{613, 42893, kSentinel}}, {{614, 42922, kSentinel}}, {{619, 11362, kSentinel}},  // NOLINT
  {{625, 11374, kSentinel}}, {{637, 11364, kSentinel}}, {{837, 921, 953, 8126}}, {{880, 881, kSentinel}},  // NOLINT
  {{882, 883, kSentinel}}, {{886, 887, kSentinel}}, {{891, 1021, kSentinel}}, {{893, 1023, kSentinel}},  // NOLINT
  {{902, 940, kSentinel}}, {{904, 941, kSentinel}}, {{906, 943, kSentinel}}, {{908, 972, kSentinel}},  // NOLINT
  {{910, 973, kSentinel}}, {{911, 974, kSentinel}}, {{913, 945, kSentinel}}, {{914, 946, 976, kSentinel}},  // NOLINT
  {{915, 947, kSentinel}}, {{916, 948, kSentinel}}, {{917, 949, 1013, kSentinel}}, {{918, 950, kSentinel}},  // NOLINT
  {{919, 951, kSentinel}}, {{920, 952, 977, kSentinel}}, {{922, 954, 1008, kSentinel}}, {{923, 955, kSentinel}},  // NOLINT
  {{925, 957, kSentinel}}, {{927, 959, kSentinel}}, {{928, 960, 982, kSentinel}}, {{929, 961, 1009, kSentinel}},  // NOLINT
  {{931, 962, 963, kSentinel}}, {{932, 964, kSentinel}}, {{933, 965, kSentinel}}, {{934, 966, 981, kSentinel}},  // NOLINT
  {{935, 967, kSentinel}}, {{939, 971, kSentinel}}, {{975, 983, kSentinel}}, {{984, 985, kSentinel}},  // NOLINT
  {{986, 987, kSentinel}}, {{988, 989, kSentinel}}, {{990, 991, kSentinel}}, {{992, 993, kSentinel}},  // NOLINT
  {{994, 995, kSentinel}}, {{996, 997, kSentinel}}, {{998, 999, kSentinel}}, {{1000, 1001, kSentinel}},  // NOLINT
  {{1002, 1003, kSentinel}}, {{1004, 1005, kSentinel}}, {{1006, 1007, kSentinel}}, {{1010, 1017, kSentinel}},  // NOLINT
  {{1015, 1016, kSentinel}}, {{1018, 1019, kSentinel}}, {{1024, 1104, kSentinel}}, {{1039, 1119, kSentinel}},  // NOLINT
  {{1040, 1072, kSentinel}}, {{1071, 1103, kSentinel}}, {{1120, 1121, kSentinel}}, {{1122, 1123, kSentinel}},  // NOLINT
  {{1124, 1125, kSentinel}}, {{1126, 1127, kSentinel}}, {{1128, 1129, kSentinel}}, {{1130, 1131, kSentinel}},  // NOLINT
  {{1132, 1133, kSentinel}}, {{1134, 1135, kSentinel}}, {{1136, 1137, kSentinel}}, {{1138, 1139, kSentinel}},  // NOLINT
  {{1140, 1141, kSentinel}}, {{1142, 1143, kSentinel}}, {{1144, 1145, kSentinel}}, {{1146, 1147, kSentinel}},  // NOLINT
  {{1148, 1149, kSentinel}}, {{1150, 1151, kSentinel}}, {{1152, 1153, kSentinel}}, {{1162, 1163, kSentinel}},  // NOLINT
  {{1164, 1165, kSentinel}}, {{1166, 1167, kSentinel}}, {{1168, 1169, kSentinel}}, {{1170, 1171, kSentinel}},  // NOLINT
  {{1172, 1173, kSentinel}}, {{1174, 1175, kSentinel}}, {{1176, 1177, kSentinel}}, {{1178, 1179, kSentinel}},  // NOLINT
  {{1180, 1181, kSentinel}}, {{1182, 1183, kSentinel}}, {{1184, 1185, kSentinel}}, {{1186, 1187, kSentinel}},  // NOLINT
  {{1188, 1189, kSentinel}}, {{1190, 1191, kSentinel}}, {{1192, 1193, kSentinel}}, {{1194, 1195, kSentinel}},  // NOLINT
  {{1196, 1197, kSentinel}}, {{1198, 1199, kSentinel}}, {{1200, 1201, kSentinel}}, {{1202, 1203, kSentinel}},  // NOLINT
  {{1204, 1205, kSentinel}}, {{1206, 1207, kSentinel}}, {{1208, 1209, kSentinel}}, {{1210, 1211, kSentinel}},  // NOLINT
  {{1212, 1213, kSentinel}}, {{1214, 1215, kSentinel}}, {{1216, 1231, kSentinel}}, {{1217, 1218, kSentinel}},  // NOLINT
  {{1219, 1220, kSentinel}}, {{1221, 1222, kSentinel}}, {{1223, 1224, kSentinel}}, {{1225, 1226, kSentinel}},  // NOLINT
  {{1227, 1228, kSentinel}}, {{1229, 1230, kSentinel}}, {{1232, 1233, kSentinel}}, {{1234, 1235, kSentinel}},  // NOLINT
  {{1236, 1237, kSentinel}}, {{1238, 1239, kSentinel}}, {{1240, 1241, kSentinel}}, {{1242, 1243, kSentinel}},  // NOLINT
  {{1244, 1245, kSentinel}}, {{1246, 1247, kSentinel}}, {{1248, 1249, kSentinel}}, {{1250, 1251, kSentinel}},  // NOLINT
  {{1252, 1253, kSentinel}}, {{1254, 1255, kSentinel}}, {{1256, 1257, kSentinel}}, {{1258, 1259, kSentinel}},  // NOLINT
  {{1260, 1261, kSentinel}}, {{1262, 1263, kSentinel}}, {{1264, 1265, kSentinel}}, {{1266, 1267, kSentinel}},  // NOLINT
  {{1268, 1269, kSentinel}}, {{1270, 1271, kSentinel}}, {{1272, 1273, kSentinel}}, {{1274, 1275, kSentinel}},  // NOLINT
  {{1276, 1277, kSentinel}}, {{1278, 1279, kSentinel}}, {{1280, 1281, kSentinel}}, {{1282, 1283, kSentinel}},  // NOLINT
  {{1284, 1285, kSentinel}}, {{1286, 1287, kSentinel}}, {{1288, 1289, kSentinel}}, {{1290, 1291, kSentinel}},  // NOLINT
  {{1292, 1293, kSentinel}}, {{1294, 1295, kSentinel}}, {{1296, 1297, kSentinel}}, {{1298, 1299, kSentinel}},  // NOLINT
  {{1300, 1301, kSentinel}}, {{1302, 1303, kSentinel}}, {{1304, 1305, kSentinel}}, {{1306, 1307, kSentinel}},  // NOLINT
  {{1308, 1309, kSentinel}}, {{1310, 1311, kSentinel}}, {{1312, 1313, kSentinel}}, {{1314, 1315, kSentinel}},  // NOLINT
  {{1316, 1317, kSentinel}}, {{1318, 1319, kSentinel}}, {{1329, 1377, kSentinel}}, {{1366, 1414, kSentinel}},  // NOLINT
  {{4256, 11520, kSentinel}}, {{4293, 11557, kSentinel}}, {{4295, 11559, kSentinel}}, {{4301, 11565, kSentinel}},  // NOLINT
  {{7545, 42877, kSentinel}}, {{7549, 11363, kSentinel}}, {{7680, 7681, kSentinel}}, {{7682, 7683, kSentinel}},  // NOLINT
  {{7684, 7685, kSentinel}}, {{7686, 7687, kSentinel}}, {{7688, 7689, kSentinel}}, {{7690, 7691, kSentinel}},  // NOLINT
  {{7692, 7693, kSentinel}}, {{7694, 7695, kSentinel}}, {{7696, 7697, kSentinel}}, {{7698, 7699, kSentinel}},  // NOLINT
  {{7700, 7701, kSentinel}}, {{7702, 7703, kSentinel}}, {{7704, 7705, kSentinel}}, {{7706, 7707, kSentinel}},  // NOLINT
  {{7708, 7709, kSentinel}}, {{7710, 7711, kSentinel}}, {{7712, 7713, kSentinel}}, {{7714, 7715, kSentinel}},  // NOLINT
  {{7716, 7717, kSentinel}}, {{7718, 7719, kSentinel}}, {{7720, 7721, kSentinel}}, {{7722, 7723, kSentinel}},  // NOLINT
  {{7724, 7725, kSentinel}}, {{7726, 7727, kSentinel}}, {{7728, 7729, kSentinel}}, {{7730, 7731, kSentinel}},  // NOLINT
  {{7732, 7733, kSentinel}}, {{7734, 7735, kSentinel}}, {{7736, 7737, kSentinel}}, {{7738, 7739, kSentinel}},  // NOLINT
  {{7740, 7741, kSentinel}}, {{7742, 7743, kSentinel}}, {{7744, 7745, kSentinel}}, {{7746, 7747, kSentinel}},  // NOLINT
  {{7748, 7749, kSentinel}}, {{7750, 7751, kSentinel}}, {{7752, 7753, kSentinel}}, {{7754, 7755, kSentinel}},  // NOLINT
  {{7756, 7757, kSentinel}}, {{7758, 7759, kSentinel}}, {{7760, 7761, kSentinel}}, {{7762, 7763, kSentinel}},  // NOLINT
  {{7764, 7765, kSentinel}}, {{7766, 7767, kSentinel}}, {{7768, 7769, kSentinel}}, {{7770, 7771, kSentinel}},  // NOLINT
  {{7772, 7773, kSentinel}}, {{7774, 7775, kSentinel}}, {{7776, 7777, 7835, kSentinel}}, {{7778, 7779, kSentinel}},  // NOLINT
  {{7780, 7781, kSentinel}}, {{7782, 7783, kSentinel}}, {{7784, 7785, kSentinel}}, {{7786, 7787, kSentinel}},  // NOLINT
  {{7788, 7789, kSentinel}}, {{7790, 7791, kSentinel}}, {{7792, 7793, kSentinel}}, {{7794, 7795, kSentinel}},  // NOLINT
  {{7796, 7797, kSentinel}}, {{7798, 7799, kSentinel}}, {{7800, 7801, kSentinel}}, {{7802, 7803, kSentinel}},  // NOLINT
  {{7804, 7805, kSentinel}}, {{7806, 7807, kSentinel}}, {{7808, 7809, kSentinel}}, {{7810, 7811, kSentinel}},  // NOLINT
  {{7812, 7813, kSentinel}}, {{7814, 7815, kSentinel}}, {{7816, 7817, kSentinel}}, {{7818, 7819, kSentinel}},  // NOLINT
  {{7820, 7821, kSentinel}}, {{7822, 7823, kSentinel}}, {{7824, 7825, kSentinel}}, {{7826, 7827, kSentinel}},  // NOLINT
  {{7828, 7829, kSentinel}}, {{7840, 7841, kSentinel}}, {{7842, 7843, kSentinel}}, {{7844, 7845, kSentinel}},  // NOLINT
  {{7846, 7847, kSentinel}}, {{7848, 7849, kSentinel}}, {{7850, 7851, kSentinel}}, {{7852, 7853, kSentinel}},  // NOLINT
  {{7854, 7855, kSentinel}}, {{7856, 7857, kSentinel}}, {{7858, 7859, kSentinel}}, {{7860, 7861, kSentinel}},  // NOLINT
  {{7862, 7863, kSentinel}}, {{7864, 7865, kSentinel}}, {{7866, 7867, kSentinel}}, {{7868, 7869, kSentinel}},  // NOLINT
  {{7870, 7871, kSentinel}}, {{7872, 7873, kSentinel}}, {{7874, 7875, kSentinel}}, {{7876, 7877, kSentinel}},  // NOLINT
  {{7878, 7879, kSentinel}}, {{7880, 7881, kSentinel}}, {{7882, 7883, kSentinel}}, {{7884, 7885, kSentinel}},  // NOLINT
  {{7886, 7887, kSentinel}}, {{7888, 7889, kSentinel}}, {{7890, 7891, kSentinel}}, {{7892, 7893, kSentinel}},  // NOLINT
  {{7894, 7895, kSentinel}}, {{7896, 7897, kSentinel}}, {{7898, 7899, kSentinel}}, {{7900, 7901, kSentinel}},  // NOLINT
  {{7902, 7903, kSentinel}}, {{7904, 7905, kSentinel}}, {{7906, 7907, kSentinel}}, {{7908, 7909, kSentinel}},  // NOLINT
  {{7910, 7911, kSentinel}}, {{7912, 7913, kSentinel}}, {{7914, 7915, kSentinel}}, {{7916, 7917, kSentinel}},  // NOLINT
  {{7918, 7919, kSentinel}}, {{7920, 7921, kSentinel}}, {{7922, 7923, kSentinel}}, {{7924, 7925, kSentinel}},  // NOLINT
  {{7926, 7927, kSentinel}}, {{7928, 7929, kSentinel}}, {{7930, 7931, kSentinel}}, {{7932, 7933, kSentinel}},  // NOLINT
  {{7934, 7935, kSentinel}}, {{7936, 7944, kSentinel}}, {{7943, 7951, kSentinel}}, {{7952, 7960, kSentinel}},  // NOLINT
  {{7957, 7965, kSentinel}}, {{7968, 7976, kSentinel}}, {{7975, 7983, kSentinel}}, {{7984, 7992, kSentinel}},  // NOLINT
  {{7991, 7999, kSentinel}}, {{8000, 8008, kSentinel}}, {{8005, 8013, kSentinel}}, {{8017, 8025, kSentinel}},  // NOLINT
  {{8019, 8027, kSentinel}}, {{8021, 8029, kSentinel}}, {{8023, 8031, kSentinel}}, {{8032, 8040, kSentinel}},  // NOLINT
  {{8039, 8047, kSentinel}}, {{8048, 8122, kSentinel}}, {{8049, 8123, kSentinel}}, {{8050, 8136, kSentinel}},  // NOLINT
  {{8053, 8139, kSentinel}}, {{8054, 8154, kSentinel}}, {{8055, 8155, kSentinel}}, {{8056, 8184, kSentinel}},  // NOLINT
  {{8057, 8185, kSentinel}}, {{8058, 8170, kSentinel}}, {{8059, 8171, kSentinel}}, {{8060, 8186, kSentinel}},  // NOLINT
  {{8061, 8187, kSentinel}}, {{8112, 8120, kSentinel}}, {{8113, 8121, kSentinel}}, {{8144, 8152, kSentinel}},  // NOLINT
  {{8145, 8153, kSentinel}}, {{8160, 8168, kSentinel}}, {{8161, 8169, kSentinel}}, {{8165, 8172, kSentinel}},  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262UnCanonicalizeTable0Size = 990;  // NOLINT
static const int32_t kEcma262UnCanonicalizeTable0[1980] = {
  1073741889, 1, 90, 5, 1073741921, 1, 122, 5, 181, 9, 1073742016, 13, 214, 17, 1073742040, 21,  // NOLINT
  222, 25, 1073742048, 13, 246, 17, 1073742072, 21, 254, 25, 255, 29, 256, 33, 257, 33,  // NOLINT
  258, 37, 259, 37, 260, 41, 261, 41, 262, 45, 263, 45, 264, 49, 265, 49,  // NOLINT
  266, 53, 267, 53, 268, 57, 269, 57, 270, 61, 271, 61, 272, 65, 273, 65,  // NOLINT
  274, 69, 275, 69, 276, 73, 277, 73, 278, 77, 279, 77, 280, 81, 281, 81,  // NOLINT
  282, 85, 283, 85, 284, 89, 285, 89, 286, 93, 287, 93, 288, 97, 289, 97,  // NOLINT
  290, 101, 291, 101, 292, 105, 293, 105, 294, 109, 295, 109, 296, 113, 297, 113,  // NOLINT
  298, 117, 299, 117, 300, 121, 301, 121, 302, 125, 303, 125, 306, 129, 307, 129,  // NOLINT
  308, 133, 309, 133, 310, 137, 311, 137, 313, 141, 314, 141, 315, 145, 316, 145,  // NOLINT
  317, 149, 318, 149, 319, 153, 320, 153, 321, 157, 322, 157, 323, 161, 324, 161,  // NOLINT
  325, 165, 326, 165, 327, 169, 328, 169, 330, 173, 331, 173, 332, 177, 333, 177,  // NOLINT
  334, 181, 335, 181, 336, 185, 337, 185, 338, 189, 339, 189, 340, 193, 341, 193,  // NOLINT
  342, 197, 343, 197, 344, 201, 345, 201, 346, 205, 347, 205, 348, 209, 349, 209,  // NOLINT
  350, 213, 351, 213, 352, 217, 353, 217, 354, 221, 355, 221, 356, 225, 357, 225,  // NOLINT
  358, 229, 359, 229, 360, 233, 361, 233, 362, 237, 363, 237, 364, 241, 365, 241,  // NOLINT
  366, 245, 367, 245, 368, 249, 369, 249, 370, 253, 371, 253, 372, 257, 373, 257,  // NOLINT
  374, 261, 375, 261, 376, 29, 377, 265, 378, 265, 379, 269, 380, 269, 381, 273,  // NOLINT
  382, 273, 384, 277, 385, 281, 386, 285, 387, 285, 388, 289, 389, 289, 390, 293,  // NOLINT
  391, 297, 392, 297, 1073742217, 301, 394, 305, 395, 309, 396, 309, 398, 313, 399, 317,  // NOLINT
  400, 321, 401, 325, 402, 325, 403, 329, 404, 333, 405, 337, 406, 341, 407, 345,  // NOLINT
  408, 349, 409, 349, 410, 353, 412, 357, 413, 361, 414, 365, 415, 369, 416, 373,  // NOLINT
  417, 373, 418, 377, 419, 377, 420, 381, 421, 381, 422, 385, 423, 389, 424, 389,  // NOLINT
  425, 393, 428, 397, 429, 397, 430, 401, 431, 405, 432, 405, 1073742257, 409, 434, 413,  // NOLINT
  435, 417, 436, 417, 437, 421, 438, 421, 439, 425, 440, 429, 441, 429, 444, 433,  // NOLINT
  445, 433, 447, 437, 452, 441, 453, 441, 454, 441, 455, 445, 456, 445, 457, 445,  // NOLINT
  458, 449, 459, 449, 460, 449, 461, 453, 462, 453, 463, 457, 464, 457, 465, 461,  // NOLINT
  466, 461, 467, 465, 468, 465, 469, 469, 470, 469, 471, 473, 472, 473, 473, 477,  // NOLINT
  474, 477, 475, 481, 476, 481, 477, 313, 478, 485, 479, 485, 480, 489, 481, 489,  // NOLINT
  482, 493, 483, 493, 484, 497, 485, 497, 486, 501, 487, 501, 488, 505, 489, 505,  // NOLINT
  490, 509, 491, 509, 492, 513, 493, 513, 494, 517, 495, 517, 497, 521, 498, 521,  // NOLINT
  499, 521, 500, 525, 501, 525, 502, 337, 503, 437, 504, 529, 505, 529, 506, 533,  // NOLINT
  507, 533, 508, 537, 509, 537, 510, 541, 511, 541, 512, 545, 513, 545, 514, 549,  // NOLINT
  515, 549, 516, 553, 517, 553, 518, 557, 519, 557, 520, 561, 521, 561, 522, 565,  // NOLINT
  523, 565, 524, 569, 525, 569, 526, 573, 527, 573, 528, 577, 529, 577, 530, 581,  // NOLINT
  531, 581, 532, 585, 533, 585, 534, 589, 535, 589, 536, 593, 537, 593, 538, 597,  // NOLINT
  539, 597, 540, 601, 541, 601, 542, 605, 543, 605, 544, 365, 546, 609, 547, 609,  // NOLINT
  548, 613, 549, 613, 550, 617, 551, 617, 552, 621, 553, 621, 554, 625, 555, 625,  // NOLINT
  556, 629, 557, 629, 558, 633, 559, 633, 560, 637, 561, 637, 562, 641, 563, 641,  // NOLINT
  570, 645, 571, 649, 572, 649, 573, 353, 574, 653, 1073742399, 657, 576, 661, 577, 665,  // NOLINT
  578, 665, 579, 277, 580, 669, 581, 673, 582, 677, 583, 677, 584, 681, 585, 681,  // NOLINT
  586, 685, 587, 685, 588, 689, 589, 689, 590, 693, 591, 693, 592, 697, 593, 701,  // NOLINT
  594, 705, 595, 281, 596, 293, 1073742422, 301, 599, 305, 601, 317, 603, 321, 608, 329,  // NOLINT
  611, 333, 613, 709, 614, 713, 616, 345, 617, 341, 619, 717, 623, 357, 625, 721,  // NOLINT
  626, 361, 629, 369, 637, 725, 640, 385, 643, 393, 648, 401, 649, 669, 1073742474, 409,  // NOLINT
  651, 413, 652, 673, 658, 425, 837, 729, 880, 733, 881, 733, 882, 737, 883, 737,  // NOLINT
  886, 741, 887, 741, 1073742715, 745, 893, 749, 902, 753, 1073742728, 757, 906, 761, 908, 765,  // NOLINT
  1073742734, 769, 911, 773, 913, 777, 914, 781, 1073742739, 785, 916, 789, 917, 793, 1073742742, 797,  // NOLINT
  919, 801, 920, 805, 921, 729, 922, 809, 923, 813, 924, 9, 1073742749, 817, 927, 821,  // NOLINT
  928, 825, 929, 829, 931, 833, 1073742756, 837, 933, 841, 934, 845, 1073742759, 849, 939, 853,  // NOLINT
  940, 753, 1073742765, 757, 943, 761, 945, 777, 946, 781, 1073742771, 785, 948, 789, 949, 793,  // NOLINT
  1073742774, 797, 951, 801, 952, 805, 953, 729, 954, 809, 955, 813, 956, 9, 1073742781, 817,  // NOLINT
  959, 821, 960, 825, 961, 829, 962, 833, 963, 833, 1073742788, 837, 965, 841, 966, 845,  // NOLINT
  1073742791, 849, 971, 853, 972, 765, 1073742797, 769, 974, 773, 975, 857, 976, 781, 977, 805,  // NOLINT
  981, 845, 982, 825, 983, 857, 984, 861, 985, 861, 986, 865, 987, 865, 988, 869,  // NOLINT
  989, 869, 990, 873, 991, 873, 992, 877, 993, 877, 994, 881, 995, 881, 996, 885,  // NOLINT
  997, 885, 998, 889, 999, 889, 1000, 893, 1001, 893, 1002, 897, 1003, 897, 1004, 901,  // NOLINT
  1005, 901, 1006, 905, 1007, 905, 1008, 809, 1009, 829, 1010, 909, 1013, 793, 1015, 913,  // NOLINT
  1016, 913, 1017, 909, 1018, 917, 1019, 917, 1073742845, 745, 1023, 749, 1073742848, 921, 1039, 925,  // NOLINT
  1073742864, 929, 1071, 933, 1073742896, 929, 1103, 933, 1073742928, 921, 1119, 925, 1120, 937, 1121, 937,  // NOLINT
  1122, 941, 1123, 941, 1124, 945, 1125, 945, 1126, 949, 1127, 949, 1128, 953, 1129, 953,  // NOLINT
  1130, 957, 1131, 957, 1132, 961, 1133, 961, 1134, 965, 1135, 965, 1136, 969, 1137, 969,  // NOLINT
  1138, 973, 1139, 973, 1140, 977, 1141, 977, 1142, 981, 1143, 981, 1144, 985, 1145, 985,  // NOLINT
  1146, 989, 1147, 989, 1148, 993, 1149, 993, 1150, 997, 1151, 997, 1152, 1001, 1153, 1001,  // NOLINT
  1162, 1005, 1163, 1005, 1164, 1009, 1165, 1009, 1166, 1013, 1167, 1013, 1168, 1017, 1169, 1017,  // NOLINT
  1170, 1021, 1171, 1021, 1172, 1025, 1173, 1025, 1174, 1029, 1175, 1029, 1176, 1033, 1177, 1033,  // NOLINT
  1178, 1037, 1179, 1037, 1180, 1041, 1181, 1041, 1182, 1045, 1183, 1045, 1184, 1049, 1185, 1049,  // NOLINT
  1186, 1053, 1187, 1053, 1188, 1057, 1189, 1057, 1190, 1061, 1191, 1061, 1192, 1065, 1193, 1065,  // NOLINT
  1194, 1069, 1195, 1069, 1196, 1073, 1197, 1073, 1198, 1077, 1199, 1077, 1200, 1081, 1201, 1081,  // NOLINT
  1202, 1085, 1203, 1085, 1204, 1089, 1205, 1089, 1206, 1093, 1207, 1093, 1208, 1097, 1209, 1097,  // NOLINT
  1210, 1101, 1211, 1101, 1212, 1105, 1213, 1105, 1214, 1109, 1215, 1109, 1216, 1113, 1217, 1117,  // NOLINT
  1218, 1117, 1219, 1121, 1220, 1121, 1221, 1125, 1222, 1125, 1223, 1129, 1224, 1129, 1225, 1133,  // NOLINT
  1226, 1133, 1227, 1137, 1228, 1137, 1229, 1141, 1230, 1141, 1231, 1113, 1232, 1145, 1233, 1145,  // NOLINT
  1234, 1149, 1235, 1149, 1236, 1153, 1237, 1153, 1238, 1157, 1239, 1157, 1240, 1161, 1241, 1161,  // NOLINT
  1242, 1165, 1243, 1165, 1244, 1169, 1245, 1169, 1246, 1173, 1247, 1173, 1248, 1177, 1249, 1177,  // NOLINT
  1250, 1181, 1251, 1181, 1252, 1185, 1253, 1185, 1254, 1189, 1255, 1189, 1256, 1193, 1257, 1193,  // NOLINT
  1258, 1197, 1259, 1197, 1260, 1201, 1261, 1201, 1262, 1205, 1263, 1205, 1264, 1209, 1265, 1209,  // NOLINT
  1266, 1213, 1267, 1213, 1268, 1217, 1269, 1217, 1270, 1221, 1271, 1221, 1272, 1225, 1273, 1225,  // NOLINT
  1274, 1229, 1275, 1229, 1276, 1233, 1277, 1233, 1278, 1237, 1279, 1237, 1280, 1241, 1281, 1241,  // NOLINT
  1282, 1245, 1283, 1245, 1284, 1249, 1285, 1249, 1286, 1253, 1287, 1253, 1288, 1257, 1289, 1257,  // NOLINT
  1290, 1261, 1291, 1261, 1292, 1265, 1293, 1265, 1294, 1269, 1295, 1269, 1296, 1273, 1297, 1273,  // NOLINT
  1298, 1277, 1299, 1277, 1300, 1281, 1301, 1281, 1302, 1285, 1303, 1285, 1304, 1289, 1305, 1289,  // NOLINT
  1306, 1293, 1307, 1293, 1308, 1297, 1309, 1297, 1310, 1301, 1311, 1301, 1312, 1305, 1313, 1305,  // NOLINT
  1314, 1309, 1315, 1309, 1316, 1313, 1317, 1313, 1318, 1317, 1319, 1317, 1073743153, 1321, 1366, 1325,  // NOLINT
  1073743201, 1321, 1414, 1325, 1073746080, 1329, 4293, 1333, 4295, 1337, 4301, 1341, 7545, 1345, 7549, 1349,  // NOLINT
  7680, 1353, 7681, 1353, 7682, 1357, 7683, 1357, 7684, 1361, 7685, 1361, 7686, 1365, 7687, 1365,  // NOLINT
  7688, 1369, 7689, 1369, 7690, 1373, 7691, 1373, 7692, 1377, 7693, 1377, 7694, 1381, 7695, 1381,  // NOLINT
  7696, 1385, 7697, 1385, 7698, 1389, 7699, 1389, 7700, 1393, 7701, 1393, 7702, 1397, 7703, 1397,  // NOLINT
  7704, 1401, 7705, 1401, 7706, 1405, 7707, 1405, 7708, 1409, 7709, 1409, 7710, 1413, 7711, 1413,  // NOLINT
  7712, 1417, 7713, 1417, 7714, 1421, 7715, 1421, 7716, 1425, 7717, 1425, 7718, 1429, 7719, 1429,  // NOLINT
  7720, 1433, 7721, 1433, 7722, 1437, 7723, 1437, 7724, 1441, 7725, 1441, 7726, 1445, 7727, 1445,  // NOLINT
  7728, 1449, 7729, 1449, 7730, 1453, 7731, 1453, 7732, 1457, 7733, 1457, 7734, 1461, 7735, 1461,  // NOLINT
  7736, 1465, 7737, 1465, 7738, 1469, 7739, 1469, 7740, 1473, 7741, 1473, 7742, 1477, 7743, 1477,  // NOLINT
  7744, 1481, 7745, 1481, 7746, 1485, 7747, 1485, 7748, 1489, 7749, 1489, 7750, 1493, 7751, 1493,  // NOLINT
  7752, 1497, 7753, 1497, 7754, 1501, 7755, 1501, 7756, 1505, 7757, 1505, 7758, 1509, 7759, 1509,  // NOLINT
  7760, 1513, 7761, 1513, 7762, 1517, 7763, 1517, 7764, 1521, 7765, 1521, 7766, 1525, 7767, 1525,  // NOLINT
  7768, 1529, 7769, 1529, 7770, 1533, 7771, 1533, 7772, 1537, 7773, 1537, 7774, 1541, 7775, 1541,  // NOLINT
  7776, 1545, 7777, 1545, 7778, 1549, 7779, 1549, 7780, 1553, 7781, 1553, 7782, 1557, 7783, 1557,  // NOLINT
  7784, 1561, 7785, 1561, 7786, 1565, 7787, 1565, 7788, 1569, 7789, 1569, 7790, 1573, 7791, 1573,  // NOLINT
  7792, 1577, 7793, 1577, 7794, 1581, 7795, 1581, 7796, 1585, 7797, 1585, 7798, 1589, 7799, 1589,  // NOLINT
  7800, 1593, 7801, 1593, 7802, 1597, 7803, 1597, 7804, 1601, 7805, 1601, 7806, 1605, 7807, 1605,  // NOLINT
  7808, 1609, 7809, 1609, 7810, 1613, 7811, 1613, 7812, 1617, 7813, 1617, 7814, 1621, 7815, 1621,  // NOLINT
  7816, 1625, 7817, 1625, 7818, 1629, 7819, 1629, 7820, 1633, 7821, 1633, 7822, 1637, 7823, 1637,  // NOLINT
  7824, 1641, 7825, 1641, 7826, 1645, 7827, 1645, 7828, 1649, 7829, 1649, 7835, 1545, 7840, 1653,  // NOLINT
  7841, 1653, 7842, 1657, 7843, 1657, 7844, 1661, 7845, 1661, 7846, 1665, 7847, 1665, 7848, 1669,  // NOLINT
  7849, 1669, 7850, 1673, 7851, 1673, 7852, 1677, 7853, 1677, 7854, 1681, 7855, 1681, 7856, 1685,  // NOLINT
  7857, 1685, 7858, 1689, 7859, 1689, 7860, 1693, 7861, 1693, 7862, 1697, 7863, 1697, 7864, 1701,  // NOLINT
  7865, 1701, 7866, 1705, 7867, 1705, 7868, 1709, 7869, 1709, 7870, 1713, 7871, 1713, 7872, 1717,  // NOLINT
  7873, 1717, 7874, 1721, 7875, 1721, 7876, 1725, 7877, 1725, 7878, 1729, 7879, 1729, 7880, 1733,  // NOLINT
  7881, 1733, 7882, 1737, 7883, 1737, 7884, 1741, 7885, 1741, 7886, 1745, 7887, 1745, 7888, 1749,  // NOLINT
  7889, 1749, 7890, 1753, 7891, 1753, 7892, 1757, 7893, 1757, 7894, 1761, 7895, 1761, 7896, 1765,  // NOLINT
  7897, 1765, 7898, 1769, 7899, 1769, 7900, 1773, 7901, 1773, 7902, 1777, 7903, 1777, 7904, 1781,  // NOLINT
  7905, 1781, 7906, 1785, 7907, 1785, 7908, 1789, 7909, 1789, 7910, 1793, 7911, 1793, 7912, 1797,  // NOLINT
  7913, 1797, 7914, 1801, 7915, 1801, 7916, 1805, 7917, 1805, 7918, 1809, 7919, 1809, 7920, 1813,  // NOLINT
  7921, 1813, 7922, 1817, 7923, 1817, 7924, 1821, 7925, 1821, 7926, 1825, 7927, 1825, 7928, 1829,  // NOLINT
  7929, 1829, 7930, 1833, 7931, 1833, 7932, 1837, 7933, 1837, 7934, 1841, 7935, 1841, 1073749760, 1845,  // NOLINT
  7943, 1849, 1073749768, 1845, 7951, 1849, 1073749776, 1853, 7957, 1857, 1073749784, 1853, 7965, 1857, 1073749792, 1861,  // NOLINT
  7975, 1865, 1073749800, 1861, 7983, 1865, 1073749808, 1869, 7991, 1873, 1073749816, 1869, 7999, 1873, 1073749824, 1877,  // NOLINT
  8005, 1881, 1073749832, 1877, 8013, 1881, 8017, 1885, 8019, 1889, 8021, 1893, 8023, 1897, 8025, 1885,  // NOLINT
  8027, 1889, 8029, 1893, 8031, 1897, 1073749856, 1901, 8039, 1905, 1073749864, 1901, 8047, 1905, 1073749872, 1909,  // NOLINT
  8049, 1913, 1073749874, 1917, 8053, 1921, 1073749878, 1925, 8055, 1929, 1073749880, 1933, 8057, 1937, 1073749882, 1941,  // NOLINT
  8059, 1945, 1073749884, 1949, 8061, 1953, 1073749936, 1957, 8113, 1961, 1073749944, 1957, 8121, 1961, 1073749946, 1909,  // NOLINT
  8123, 1913, 8126, 729, 1073749960, 1917, 8139, 1921, 1073749968, 1965, 8145, 1969, 1073749976, 1965, 8153, 1969,  // NOLINT
  1073749978, 1925, 8155, 1929, 1073749984, 1973, 8161, 1977, 8165, 1981, 1073749992, 1973, 8169, 1977, 1073749994, 1941,  // NOLINT
  8171, 1945, 8172, 1981, 1073750008, 1933, 8185, 1937, 1073750010, 1949, 8187, 1953 };  // NOLINT
static const uint16_t kEcma262UnCanonicalizeMultiStrings0Size = 497;  // NOLINT
static const MultiCharacterSpecialCase<2> kEcma262UnCanonicalizeMultiStrings1[83] = {  // NOLINT
  {{8498, 8526}}, {{8544, 8560}}, {{8559, 8575}}, {{8579, 8580}},  // NOLINT
  {{9398, 9424}}, {{9423, 9449}}, {{11264, 11312}}, {{11310, 11358}},  // NOLINT
  {{11360, 11361}}, {{619, 11362}}, {{7549, 11363}}, {{637, 11364}},  // NOLINT
  {{570, 11365}}, {{574, 11366}}, {{11367, 11368}}, {{11369, 11370}},  // NOLINT
  {{11371, 11372}}, {{593, 11373}}, {{625, 11374}}, {{592, 11375}},  // NOLINT
  {{594, 11376}}, {{11378, 11379}}, {{11381, 11382}}, {{575, 11390}},  // NOLINT
  {{576, 11391}}, {{11392, 11393}}, {{11394, 11395}}, {{11396, 11397}},  // NOLINT
  {{11398, 11399}}, {{11400, 11401}}, {{11402, 11403}}, {{11404, 11405}},  // NOLINT
  {{11406, 11407}}, {{11408, 11409}}, {{11410, 11411}}, {{11412, 11413}},  // NOLINT
  {{11414, 11415}}, {{11416, 11417}}, {{11418, 11419}}, {{11420, 11421}},  // NOLINT
  {{11422, 11423}}, {{11424, 11425}}, {{11426, 11427}}, {{11428, 11429}},  // NOLINT
  {{11430, 11431}}, {{11432, 11433}}, {{11434, 11435}}, {{11436, 11437}},  // NOLINT
  {{11438, 11439}}, {{11440, 11441}}, {{11442, 11443}}, {{11444, 11445}},  // NOLINT
  {{11446, 11447}}, {{11448, 11449}}, {{11450, 11451}}, {{11452, 11453}},  // NOLINT
  {{11454, 11455}}, {{11456, 11457}}, {{11458, 11459}}, {{11460, 11461}},  // NOLINT
  {{11462, 11463}}, {{11464, 11465}}, {{11466, 11467}}, {{11468, 11469}},  // NOLINT
  {{11470, 11471}}, {{11472, 11473}}, {{11474, 11475}}, {{11476, 11477}},  // NOLINT
  {{11478, 11479}}, {{11480, 11481}}, {{11482, 11483}}, {{11484, 11485}},  // NOLINT
  {{11486, 11487}}, {{11488, 11489}}, {{11490, 11491}}, {{11499, 11500}},  // NOLINT
  {{11501, 11502}}, {{11506, 11507}}, {{4256, 11520}}, {{4293, 11557}},  // NOLINT
  {{4295, 11559}}, {{4301, 11565}}, {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262UnCanonicalizeTable1Size = 149;  // NOLINT
static const int32_t kEcma262UnCanonicalizeTable1[298] = {
  306, 1, 334, 1, 1073742176, 5, 367, 9, 1073742192, 5, 383, 9, 387, 13, 388, 13,  // NOLINT
  1073743030, 17, 1231, 21, 1073743056, 17, 1257, 21, 1073744896, 25, 3118, 29, 1073744944, 25, 3166, 29,  // NOLINT
  3168, 33, 3169, 33, 3170, 37, 3171, 41, 3172, 45, 3173, 49, 3174, 53, 3175, 57,  // NOLINT
  3176, 57, 3177, 61, 3178, 61, 3179, 65, 3180, 65, 3181, 69, 3182, 73, 3183, 77,  // NOLINT
  3184, 81, 3186, 85, 3187, 85, 3189, 89, 3190, 89, 1073745022, 93, 3199, 97, 3200, 101,  // NOLINT
  3201, 101, 3202, 105, 3203, 105, 3204, 109, 3205, 109, 3206, 113, 3207, 113, 3208, 117,  // NOLINT
  3209, 117, 3210, 121, 3211, 121, 3212, 125, 3213, 125, 3214, 129, 3215, 129, 3216, 133,  // NOLINT
  3217, 133, 3218, 137, 3219, 137, 3220, 141, 3221, 141, 3222, 145, 3223, 145, 3224, 149,  // NOLINT
  3225, 149, 3226, 153, 3227, 153, 3228, 157, 3229, 157, 3230, 161, 3231, 161, 3232, 165,  // NOLINT
  3233, 165, 3234, 169, 3235, 169, 3236, 173, 3237, 173, 3238, 177, 3239, 177, 3240, 181,  // NOLINT
  3241, 181, 3242, 185, 3243, 185, 3244, 189, 3245, 189, 3246, 193, 3247, 193, 3248, 197,  // NOLINT
  3249, 197, 3250, 201, 3251, 201, 3252, 205, 3253, 205, 3254, 209, 3255, 209, 3256, 213,  // NOLINT
  3257, 213, 3258, 217, 3259, 217, 3260, 221, 3261, 221, 3262, 225, 3263, 225, 3264, 229,  // NOLINT
  3265, 229, 3266, 233, 3267, 233, 3268, 237, 3269, 237, 3270, 241, 3271, 241, 3272, 245,  // NOLINT
  3273, 245, 3274, 249, 3275, 249, 3276, 253, 3277, 253, 3278, 257, 3279, 257, 3280, 261,  // NOLINT
  3281, 261, 3282, 265, 3283, 265, 3284, 269, 3285, 269, 3286, 273, 3287, 273, 3288, 277,  // NOLINT
  3289, 277, 3290, 281, 3291, 281, 3292, 285, 3293, 285, 3294, 289, 3295, 289, 3296, 293,  // NOLINT
  3297, 293, 3298, 297, 3299, 297, 3307, 301, 3308, 301, 3309, 305, 3310, 305, 3314, 309,  // NOLINT
  3315, 309, 1073745152, 313, 3365, 317, 3367, 321, 3373, 325 };  // NOLINT
static const uint16_t kEcma262UnCanonicalizeMultiStrings1Size = 83;  // NOLINT
static const MultiCharacterSpecialCase<2> kEcma262UnCanonicalizeMultiStrings5[92] = {  // NOLINT
  {{42560, 42561}}, {{42562, 42563}}, {{42564, 42565}}, {{42566, 42567}},  // NOLINT
  {{42568, 42569}}, {{42570, 42571}}, {{42572, 42573}}, {{42574, 42575}},  // NOLINT
  {{42576, 42577}}, {{42578, 42579}}, {{42580, 42581}}, {{42582, 42583}},  // NOLINT
  {{42584, 42585}}, {{42586, 42587}}, {{42588, 42589}}, {{42590, 42591}},  // NOLINT
  {{42592, 42593}}, {{42594, 42595}}, {{42596, 42597}}, {{42598, 42599}},  // NOLINT
  {{42600, 42601}}, {{42602, 42603}}, {{42604, 42605}}, {{42624, 42625}},  // NOLINT
  {{42626, 42627}}, {{42628, 42629}}, {{42630, 42631}}, {{42632, 42633}},  // NOLINT
  {{42634, 42635}}, {{42636, 42637}}, {{42638, 42639}}, {{42640, 42641}},  // NOLINT
  {{42642, 42643}}, {{42644, 42645}}, {{42646, 42647}}, {{42786, 42787}},  // NOLINT
  {{42788, 42789}}, {{42790, 42791}}, {{42792, 42793}}, {{42794, 42795}},  // NOLINT
  {{42796, 42797}}, {{42798, 42799}}, {{42802, 42803}}, {{42804, 42805}},  // NOLINT
  {{42806, 42807}}, {{42808, 42809}}, {{42810, 42811}}, {{42812, 42813}},  // NOLINT
  {{42814, 42815}}, {{42816, 42817}}, {{42818, 42819}}, {{42820, 42821}},  // NOLINT
  {{42822, 42823}}, {{42824, 42825}}, {{42826, 42827}}, {{42828, 42829}},  // NOLINT
  {{42830, 42831}}, {{42832, 42833}}, {{42834, 42835}}, {{42836, 42837}},  // NOLINT
  {{42838, 42839}}, {{42840, 42841}}, {{42842, 42843}}, {{42844, 42845}},  // NOLINT
  {{42846, 42847}}, {{42848, 42849}}, {{42850, 42851}}, {{42852, 42853}},  // NOLINT
  {{42854, 42855}}, {{42856, 42857}}, {{42858, 42859}}, {{42860, 42861}},  // NOLINT
  {{42862, 42863}}, {{42873, 42874}}, {{42875, 42876}}, {{7545, 42877}},  // NOLINT
  {{42878, 42879}}, {{42880, 42881}}, {{42882, 42883}}, {{42884, 42885}},  // NOLINT
  {{42886, 42887}}, {{42891, 42892}}, {{613, 42893}}, {{42896, 42897}},  // NOLINT
  {{42898, 42899}}, {{42912, 42913}}, {{42914, 42915}}, {{42916, 42917}},  // NOLINT
  {{42918, 42919}}, {{42920, 42921}}, {{614, 42922}}, {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262UnCanonicalizeTable5Size = 179;  // NOLINT
static const int32_t kEcma262UnCanonicalizeTable5[358] = {
  1600, 1, 1601, 1, 1602, 5, 1603, 5, 1604, 9, 1605, 9, 1606, 13, 1607, 13,  // NOLINT
  1608, 17, 1609, 17, 1610, 21, 1611, 21, 1612, 25, 1613, 25, 1614, 29, 1615, 29,  // NOLINT
  1616, 33, 1617, 33, 1618, 37, 1619, 37, 1620, 41, 1621, 41, 1622, 45, 1623, 45,  // NOLINT
  1624, 49, 1625, 49, 1626, 53, 1627, 53, 1628, 57, 1629, 57, 1630, 61, 1631, 61,  // NOLINT
  1632, 65, 1633, 65, 1634, 69, 1635, 69, 1636, 73, 1637, 73, 1638, 77, 1639, 77,  // NOLINT
  1640, 81, 1641, 81, 1642, 85, 1643, 85, 1644, 89, 1645, 89, 1664, 93, 1665, 93,  // NOLINT
  1666, 97, 1667, 97, 1668, 101, 1669, 101, 1670, 105, 1671, 105, 1672, 109, 1673, 109,  // NOLINT
  1674, 113, 1675, 113, 1676, 117, 1677, 117, 1678, 121, 1679, 121, 1680, 125, 1681, 125,  // NOLINT
  1682, 129, 1683, 129, 1684, 133, 1685, 133, 1686, 137, 1687, 137, 1826, 141, 1827, 141,  // NOLINT
  1828, 145, 1829, 145, 1830, 149, 1831, 149, 1832, 153, 1833, 153, 1834, 157, 1835, 157,  // NOLINT
  1836, 161, 1837, 161, 1838, 165, 1839, 165, 1842, 169, 1843, 169, 1844, 173, 1845, 173,  // NOLINT
  1846, 177, 1847, 177, 1848, 181, 1849, 181, 1850, 185, 1851, 185, 1852, 189, 1853, 189,  // NOLINT
  1854, 193, 1855, 193, 1856, 197, 1857, 197, 1858, 201, 1859, 201, 1860, 205, 1861, 205,  // NOLINT
  1862, 209, 1863, 209, 1864, 213, 1865, 213, 1866, 217, 1867, 217, 1868, 221, 1869, 221,  // NOLINT
  1870, 225, 1871, 225, 1872, 229, 1873, 229, 1874, 233, 1875, 233, 1876, 237, 1877, 237,  // NOLINT
  1878, 241, 1879, 241, 1880, 245, 1881, 245, 1882, 249, 1883, 249, 1884, 253, 1885, 253,  // NOLINT
  1886, 257, 1887, 257, 1888, 261, 1889, 261, 1890, 265, 1891, 265, 1892, 269, 1893, 269,  // NOLINT
  1894, 273, 1895, 273, 1896, 277, 1897, 277, 1898, 281, 1899, 281, 1900, 285, 1901, 285,  // NOLINT
  1902, 289, 1903, 289, 1913, 293, 1914, 293, 1915, 297, 1916, 297, 1917, 301, 1918, 305,  // NOLINT
  1919, 305, 1920, 309, 1921, 309, 1922, 313, 1923, 313, 1924, 317, 1925, 317, 1926, 321,  // NOLINT
  1927, 321, 1931, 325, 1932, 325, 1933, 329, 1936, 333, 1937, 333, 1938, 337, 1939, 337,  // NOLINT
  1952, 341, 1953, 341, 1954, 345, 1955, 345, 1956, 349, 1957, 349, 1958, 353, 1959, 353,  // NOLINT
  1960, 357, 1961, 357, 1962, 361 };  // NOLINT
static const uint16_t kEcma262UnCanonicalizeMultiStrings5Size = 92;  // NOLINT
static const MultiCharacterSpecialCase<2> kEcma262UnCanonicalizeMultiStrings7[3] = {  // NOLINT
  {{65313, 65345}}, {{65338, 65370}}, {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262UnCanonicalizeTable7Size = 4;  // NOLINT
static const int32_t kEcma262UnCanonicalizeTable7[8] = {
  1073749793, 1, 7994, 5, 1073749825, 1, 8026, 5 };  // NOLINT
static const uint16_t kEcma262UnCanonicalizeMultiStrings7Size = 3;  // NOLINT
int Ecma262UnCanonicalize::Convert(uchar c,
                      uchar n,
                      uchar* result,
                      bool* allow_caching_ptr) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupMapping<true>(kEcma262UnCanonicalizeTable0,
                                           kEcma262UnCanonicalizeTable0Size,
                                           kEcma262UnCanonicalizeMultiStrings0,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 1: return LookupMapping<true>(kEcma262UnCanonicalizeTable1,
                                           kEcma262UnCanonicalizeTable1Size,
                                           kEcma262UnCanonicalizeMultiStrings1,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 5: return LookupMapping<true>(kEcma262UnCanonicalizeTable5,
                                           kEcma262UnCanonicalizeTable5Size,
                                           kEcma262UnCanonicalizeMultiStrings5,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 7: return LookupMapping<true>(kEcma262UnCanonicalizeTable7,
                                           kEcma262UnCanonicalizeTable7Size,
                                           kEcma262UnCanonicalizeMultiStrings7,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    default: return 0;
  }
}

static const MultiCharacterSpecialCase<1> kCanonicalizationRangeMultiStrings0[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kCanonicalizationRangeTable0Size = 70;  // NOLINT
static const int32_t kCanonicalizationRangeTable0[140] = {
  1073741889, 100, 90, 0, 1073741921, 100, 122, 0, 1073742016, 88, 214, 0, 1073742040, 24, 222, 0,  // NOLINT
  1073742048, 88, 246, 0, 1073742072, 24, 254, 0, 1073742715, 8, 893, 0, 1073742728, 8, 906, 0,  // NOLINT
  1073742749, 8, 927, 0, 1073742759, 16, 939, 0, 1073742765, 8, 943, 0, 1073742781, 8, 959, 0,  // NOLINT
  1073742791, 16, 971, 0, 1073742845, 8, 1023, 0, 1073742848, 60, 1039, 0, 1073742864, 124, 1071, 0,  // NOLINT
  1073742896, 124, 1103, 0, 1073742928, 60, 1119, 0, 1073743153, 148, 1366, 0, 1073743201, 148, 1414, 0,  // NOLINT
  1073746080, 148, 4293, 0, 1073749760, 28, 7943, 0, 1073749768, 28, 7951, 0, 1073749776, 20, 7957, 0,  // NOLINT
  1073749784, 20, 7965, 0, 1073749792, 28, 7975, 0, 1073749800, 28, 7983, 0, 1073749808, 28, 7991, 0,  // NOLINT
  1073749816, 28, 7999, 0, 1073749824, 20, 8005, 0, 1073749832, 20, 8013, 0, 1073749856, 28, 8039, 0,  // NOLINT
  1073749864, 28, 8047, 0, 1073749874, 12, 8053, 0, 1073749960, 12, 8139, 0 };  // NOLINT
static const uint16_t kCanonicalizationRangeMultiStrings0Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kCanonicalizationRangeMultiStrings1[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kCanonicalizationRangeTable1Size = 14;  // NOLINT
static const int32_t kCanonicalizationRangeTable1[28] = {
  1073742176, 60, 367, 0, 1073742192, 60, 383, 0, 1073743030, 100, 1231, 0, 1073743056, 100, 1257, 0,  // NOLINT
  1073744896, 184, 3118, 0, 1073744944, 184, 3166, 0, 1073745152, 148, 3365, 0 };  // NOLINT
static const uint16_t kCanonicalizationRangeMultiStrings1Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kCanonicalizationRangeMultiStrings7[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kCanonicalizationRangeTable7Size = 4;  // NOLINT
static const int32_t kCanonicalizationRangeTable7[8] = {
  1073749793, 100, 7994, 0, 1073749825, 100, 8026, 0 };  // NOLINT
static const uint16_t kCanonicalizationRangeMultiStrings7Size = 1;  // NOLINT
int CanonicalizationRange::Convert(uchar c,
                      uchar n,
                      uchar* result,
                      bool* allow_caching_ptr) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupMapping<false>(kCanonicalizationRangeTable0,
                                           kCanonicalizationRangeTable0Size,
                                           kCanonicalizationRangeMultiStrings0,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 1: return LookupMapping<false>(kCanonicalizationRangeTable1,
                                           kCanonicalizationRangeTable1Size,
                                           kCanonicalizationRangeMultiStrings1,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    case 7: return LookupMapping<false>(kCanonicalizationRangeTable7,
                                           kCanonicalizationRangeTable7Size,
                                           kCanonicalizationRangeMultiStrings7,
                                           c,
                                           n,
                                           result,
                                           allow_caching_ptr);
    default: return 0;
  }
}


const uchar UnicodeData::kMaxCodePoint = 65533;

int UnicodeData::GetByteCount() {
  return kUppercaseTable0Size * sizeof(int32_t)  // NOLINT
      + kUppercaseTable1Size * sizeof(int32_t)  // NOLINT
      + kUppercaseTable5Size * sizeof(int32_t)  // NOLINT
      + kUppercaseTable7Size * sizeof(int32_t)  // NOLINT
      + kLowercaseTable0Size * sizeof(int32_t)  // NOLINT
      + kLowercaseTable1Size * sizeof(int32_t)  // NOLINT
      + kLowercaseTable5Size * sizeof(int32_t)  // NOLINT
      + kLowercaseTable7Size * sizeof(int32_t)  // NOLINT
      + kLetterTable0Size * sizeof(int32_t)  // NOLINT
      + kLetterTable1Size * sizeof(int32_t)  // NOLINT
      + kLetterTable2Size * sizeof(int32_t)  // NOLINT
      + kLetterTable3Size * sizeof(int32_t)  // NOLINT
      + kLetterTable4Size * sizeof(int32_t)  // NOLINT
      + kLetterTable5Size * sizeof(int32_t)  // NOLINT
      + kLetterTable6Size * sizeof(int32_t)  // NOLINT
      + kLetterTable7Size * sizeof(int32_t)  // NOLINT
      + kSpaceTable0Size * sizeof(int32_t)  // NOLINT
      + kSpaceTable1Size * sizeof(int32_t)  // NOLINT
      + kNumberTable0Size * sizeof(int32_t)  // NOLINT
      + kNumberTable5Size * sizeof(int32_t)  // NOLINT
      + kNumberTable7Size * sizeof(int32_t)  // NOLINT
      + kWhiteSpaceTable0Size * sizeof(int32_t)  // NOLINT
      + kWhiteSpaceTable1Size * sizeof(int32_t)  // NOLINT
      + kLineTerminatorTable0Size * sizeof(int32_t)  // NOLINT
      + kLineTerminatorTable1Size * sizeof(int32_t)  // NOLINT
      + kCombiningMarkTable0Size * sizeof(int32_t)  // NOLINT
      + kCombiningMarkTable1Size * sizeof(int32_t)  // NOLINT
      + kCombiningMarkTable5Size * sizeof(int32_t)  // NOLINT
      + kCombiningMarkTable7Size * sizeof(int32_t)  // NOLINT
      + kConnectorPunctuationTable0Size * sizeof(int32_t)  // NOLINT
      + kConnectorPunctuationTable1Size * sizeof(int32_t)  // NOLINT
      + kConnectorPunctuationTable7Size * sizeof(int32_t)  // NOLINT
      + kToLowercaseMultiStrings0Size * sizeof(MultiCharacterSpecialCase<2>)  // NOLINT
      + kToLowercaseMultiStrings1Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kToLowercaseMultiStrings5Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kToLowercaseMultiStrings7Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kToUppercaseMultiStrings0Size * sizeof(MultiCharacterSpecialCase<3>)  // NOLINT
      + kToUppercaseMultiStrings1Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kToUppercaseMultiStrings5Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kToUppercaseMultiStrings7Size * sizeof(MultiCharacterSpecialCase<3>)  // NOLINT
      + kEcma262CanonicalizeMultiStrings0Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kEcma262CanonicalizeMultiStrings1Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kEcma262CanonicalizeMultiStrings5Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kEcma262CanonicalizeMultiStrings7Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kEcma262UnCanonicalizeMultiStrings0Size * sizeof(MultiCharacterSpecialCase<4>)  // NOLINT
      + kEcma262UnCanonicalizeMultiStrings1Size * sizeof(MultiCharacterSpecialCase<2>)  // NOLINT
      + kEcma262UnCanonicalizeMultiStrings5Size * sizeof(MultiCharacterSpecialCase<2>)  // NOLINT
      + kEcma262UnCanonicalizeMultiStrings7Size * sizeof(MultiCharacterSpecialCase<2>)  // NOLINT
      + kCanonicalizationRangeMultiStrings0Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kCanonicalizationRangeMultiStrings1Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kCanonicalizationRangeMultiStrings7Size * sizeof(MultiCharacterSpecialCase<1>); // NOLINT
}

}  // namespace unicode
