// Copyright 2007-2008 the V8 project authors. All rights reserved.
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
// This file was generated at 2011-01-03 10:57:02.088925

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
  // We only get called for non-ascii characters.
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
    // The next character is an ascii char so we scan forward over
    // the following ascii characters and return the next pure ascii
    // substring
    const byte* result = data + offset;
    offset++;
    while ((offset < str.length()) && (data[offset] <= kMaxOneByteChar))
      offset++;
    *chars_read_ptr = offset - *offset_ptr;
    *offset_ptr = offset;
    return result;
  } else {
    // The next character is non-ascii so we just fill the buffer
    unsigned cursor = 0;
    unsigned chars_read = 0;
    while (offset < str.length()) {
      uchar c = data[offset];
      if (c <= kMaxOneByteChar) {
        // Fast case for ascii characters
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

void CharacterStream::Seek(unsigned position) {
  Rewind();
  for (unsigned i = 0; i < position; i++) {
    GetNext();
  }
}

// Uppercase:            point.category == 'Lu'

static const uint16_t kUppercaseTable0Size = 430;
static const int32_t kUppercaseTable0[430] = {
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
  584, 586, 588, 590, 902, 1073742728, 906, 908,  // NOLINT
  1073742734, 911, 1073742737, 929, 1073742755, 939, 1073742802, 980,  // NOLINT
  984, 986, 988, 990, 992, 994, 996, 998,  // NOLINT
  1000, 1002, 1004, 1006, 1012, 1015, 1073742841, 1018,  // NOLINT
  1073742845, 1071, 1120, 1122, 1124, 1126, 1128, 1130,  // NOLINT
  1132, 1134, 1136, 1138, 1140, 1142, 1144, 1146,  // NOLINT
  1148, 1150, 1152, 1162, 1164, 1166, 1168, 1170,  // NOLINT
  1172, 1174, 1176, 1178, 1180, 1182, 1184, 1186,  // NOLINT
  1188, 1190, 1192, 1194, 1196, 1198, 1200, 1202,  // NOLINT
  1204, 1206, 1208, 1210, 1212, 1214, 1073743040, 1217,  // NOLINT
  1219, 1221, 1223, 1225, 1227, 1229, 1232, 1234,  // NOLINT
  1236, 1238, 1240, 1242, 1244, 1246, 1248, 1250,  // NOLINT
  1252, 1254, 1256, 1258, 1260, 1262, 1264, 1266,  // NOLINT
  1268, 1270, 1272, 1274, 1276, 1278, 1280, 1282,  // NOLINT
  1284, 1286, 1288, 1290, 1292, 1294, 1296, 1298,  // NOLINT
  1073743153, 1366, 1073746080, 4293, 7680, 7682, 7684, 7686,  // NOLINT
  7688, 7690, 7692, 7694, 7696, 7698, 7700, 7702,  // NOLINT
  7704, 7706, 7708, 7710, 7712, 7714, 7716, 7718,  // NOLINT
  7720, 7722, 7724, 7726, 7728, 7730, 7732, 7734,  // NOLINT
  7736, 7738, 7740, 7742, 7744, 7746, 7748, 7750,  // NOLINT
  7752, 7754, 7756, 7758, 7760, 7762, 7764, 7766,  // NOLINT
  7768, 7770, 7772, 7774, 7776, 7778, 7780, 7782,  // NOLINT
  7784, 7786, 7788, 7790, 7792, 7794, 7796, 7798,  // NOLINT
  7800, 7802, 7804, 7806, 7808, 7810, 7812, 7814,  // NOLINT
  7816, 7818, 7820, 7822, 7824, 7826, 7828, 7840,  // NOLINT
  7842, 7844, 7846, 7848, 7850, 7852, 7854, 7856,  // NOLINT
  7858, 7860, 7862, 7864, 7866, 7868, 7870, 7872,  // NOLINT
  7874, 7876, 7878, 7880, 7882, 7884, 7886, 7888,  // NOLINT
  7890, 7892, 7894, 7896, 7898, 7900, 7902, 7904,  // NOLINT
  7906, 7908, 7910, 7912, 7914, 7916, 7918, 7920,  // NOLINT
  7922, 7924, 7926, 7928, 1073749768, 7951, 1073749784, 7965,  // NOLINT
  1073749800, 7983, 1073749816, 7999, 1073749832, 8013, 8025, 8027,  // NOLINT
  8029, 8031, 1073749864, 8047, 1073749944, 8123, 1073749960, 8139,  // NOLINT
  1073749976, 8155, 1073749992, 8172, 1073750008, 8187 };  // NOLINT
static const uint16_t kUppercaseTable1Size = 79;
static const int32_t kUppercaseTable1[79] = {
  258, 263, 1073742091, 269, 1073742096, 274, 277, 1073742105,  // NOLINT
  285, 292, 294, 296, 1073742122, 301, 1073742128, 307,  // NOLINT
  1073742142, 319, 325, 387, 1073744896, 3118, 3168, 1073744994,  // NOLINT
  3172, 3175, 3177, 3179, 3189, 3200, 3202, 3204,  // NOLINT
  3206, 3208, 3210, 3212, 3214, 3216, 3218, 3220,  // NOLINT
  3222, 3224, 3226, 3228, 3230, 3232, 3234, 3236,  // NOLINT
  3238, 3240, 3242, 3244, 3246, 3248, 3250, 3252,  // NOLINT
  3254, 3256, 3258, 3260, 3262, 3264, 3266, 3268,  // NOLINT
  3270, 3272, 3274, 3276, 3278, 3280, 3282, 3284,  // NOLINT
  3286, 3288, 3290, 3292, 3294, 3296, 3298 };  // NOLINT
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
    case 7: return LookupPredicate(kUppercaseTable7,
                                       kUppercaseTable7Size,
                                       c);
    default: return false;
  }
}

// Lowercase:            point.category == 'Ll'

static const uint16_t kLowercaseTable0Size = 449;
static const int32_t kLowercaseTable0[449] = {
  1073741921, 122, 170, 181, 186, 1073742047, 246, 1073742072,  // NOLINT
  255, 257, 259, 261, 263, 265, 267, 269,  // NOLINT
  271, 273, 275, 277, 279, 281, 283, 285,  // NOLINT
  287, 289, 291, 293, 295, 297, 299, 301,  // NOLINT
  303, 305, 307, 309, 1073742135, 312, 314, 316,  // NOLINT
  318, 320, 322, 324, 326, 1073742152, 329, 331,  // NOLINT
  333, 335, 337, 339, 341, 343, 345, 347,  // NOLINT
  349, 351, 353, 355, 357, 359, 361, 363,  // NOLINT
  365, 367, 369, 371, 373, 375, 378, 380,  // NOLINT
  1073742206, 384, 387, 389, 392, 1073742220, 397, 402,  // NOLINT
  405, 1073742233, 411, 414, 417, 419, 421, 424,  // NOLINT
  1073742250, 427, 429, 432, 436, 438, 1073742265, 442,  // NOLINT
  1073742269, 447, 454, 457, 460, 462, 464, 466,  // NOLINT
  468, 470, 472, 474, 1073742300, 477, 479, 481,  // NOLINT
  483, 485, 487, 489, 491, 493, 1073742319, 496,  // NOLINT
  499, 501, 505, 507, 509, 511, 513, 515,  // NOLINT
  517, 519, 521, 523, 525, 527, 529, 531,  // NOLINT
  533, 535, 537, 539, 541, 543, 545, 547,  // NOLINT
  549, 551, 553, 555, 557, 559, 561, 1073742387,  // NOLINT
  569, 572, 1073742399, 576, 578, 583, 585, 587,  // NOLINT
  589, 1073742415, 659, 1073742485, 687, 1073742715, 893, 912,  // NOLINT
  1073742764, 974, 1073742800, 977, 1073742805, 983, 985, 987,  // NOLINT
  989, 991, 993, 995, 997, 999, 1001, 1003,  // NOLINT
  1005, 1073742831, 1011, 1013, 1016, 1073742843, 1020, 1073742896,  // NOLINT
  1119, 1121, 1123, 1125, 1127, 1129, 1131, 1133,  // NOLINT
  1135, 1137, 1139, 1141, 1143, 1145, 1147, 1149,  // NOLINT
  1151, 1153, 1163, 1165, 1167, 1169, 1171, 1173,  // NOLINT
  1175, 1177, 1179, 1181, 1183, 1185, 1187, 1189,  // NOLINT
  1191, 1193, 1195, 1197, 1199, 1201, 1203, 1205,  // NOLINT
  1207, 1209, 1211, 1213, 1215, 1218, 1220, 1222,  // NOLINT
  1224, 1226, 1228, 1073743054, 1231, 1233, 1235, 1237,  // NOLINT
  1239, 1241, 1243, 1245, 1247, 1249, 1251, 1253,  // NOLINT
  1255, 1257, 1259, 1261, 1263, 1265, 1267, 1269,  // NOLINT
  1271, 1273, 1275, 1277, 1279, 1281, 1283, 1285,  // NOLINT
  1287, 1289, 1291, 1293, 1295, 1297, 1299, 1073743201,  // NOLINT
  1415, 1073749248, 7467, 1073749346, 7543, 1073749369, 7578, 7681,  // NOLINT
  7683, 7685, 7687, 7689, 7691, 7693, 7695, 7697,  // NOLINT
  7699, 7701, 7703, 7705, 7707, 7709, 7711, 7713,  // NOLINT
  7715, 7717, 7719, 7721, 7723, 7725, 7727, 7729,  // NOLINT
  7731, 7733, 7735, 7737, 7739, 7741, 7743, 7745,  // NOLINT
  7747, 7749, 7751, 7753, 7755, 7757, 7759, 7761,  // NOLINT
  7763, 7765, 7767, 7769, 7771, 7773, 7775, 7777,  // NOLINT
  7779, 7781, 7783, 7785, 7787, 7789, 7791, 7793,  // NOLINT
  7795, 7797, 7799, 7801, 7803, 7805, 7807, 7809,  // NOLINT
  7811, 7813, 7815, 7817, 7819, 7821, 7823, 7825,  // NOLINT
  7827, 1073749653, 7835, 7841, 7843, 7845, 7847, 7849,  // NOLINT
  7851, 7853, 7855, 7857, 7859, 7861, 7863, 7865,  // NOLINT
  7867, 7869, 7871, 7873, 7875, 7877, 7879, 7881,  // NOLINT
  7883, 7885, 7887, 7889, 7891, 7893, 7895, 7897,  // NOLINT
  7899, 7901, 7903, 7905, 7907, 7909, 7911, 7913,  // NOLINT
  7915, 7917, 7919, 7921, 7923, 7925, 7927, 7929,  // NOLINT
  1073749760, 7943, 1073749776, 7957, 1073749792, 7975, 1073749808, 7991,  // NOLINT
  1073749824, 8005, 1073749840, 8023, 1073749856, 8039, 1073749872, 8061,  // NOLINT
  1073749888, 8071, 1073749904, 8087, 1073749920, 8103, 1073749936, 8116,  // NOLINT
  1073749942, 8119, 8126, 1073749954, 8132, 1073749958, 8135, 1073749968,  // NOLINT
  8147, 1073749974, 8151, 1073749984, 8167, 1073750002, 8180, 1073750006,  // NOLINT
  8183 };  // NOLINT
static const uint16_t kLowercaseTable1Size = 79;
static const int32_t kLowercaseTable1[79] = {
  113, 127, 266, 1073742094, 271, 275, 303, 308,  // NOLINT
  313, 1073742140, 317, 1073742150, 329, 334, 388, 1073744944,  // NOLINT
  3166, 3169, 1073744997, 3174, 3176, 3178, 3180, 3188,  // NOLINT
  1073745014, 3191, 3201, 3203, 3205, 3207, 3209, 3211,  // NOLINT
  3213, 3215, 3217, 3219, 3221, 3223, 3225, 3227,  // NOLINT
  3229, 3231, 3233, 3235, 3237, 3239, 3241, 3243,  // NOLINT
  3245, 3247, 3249, 3251, 3253, 3255, 3257, 3259,  // NOLINT
  3261, 3263, 3265, 3267, 3269, 3271, 3273, 3275,  // NOLINT
  3277, 3279, 3281, 3283, 3285, 3287, 3289, 3291,  // NOLINT
  3293, 3295, 3297, 1073745123, 3300, 1073745152, 3365 };  // NOLINT
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
    case 7: return LookupPredicate(kLowercaseTable7,
                                       kLowercaseTable7Size,
                                       c);
    default: return false;
  }
}

// Letter:               point.category in ['Lu', 'Ll', 'Lt', 'Lm', 'Lo', 'Nl' ]

static const uint16_t kLetterTable0Size = 394;
static const int32_t kLetterTable0[394] = {
  1073741889, 90, 1073741921, 122, 170, 181, 186, 1073742016,  // NOLINT
  214, 1073742040, 246, 1073742072, 705, 1073742534, 721, 1073742560,  // NOLINT
  740, 750, 1073742714, 893, 902, 1073742728, 906, 908,  // NOLINT
  1073742734, 929, 1073742755, 974, 1073742800, 1013, 1073742839, 1153,  // NOLINT
  1073742986, 1299, 1073743153, 1366, 1369, 1073743201, 1415, 1073743312,  // NOLINT
  1514, 1073743344, 1522, 1073743393, 1594, 1073743424, 1610, 1073743470,  // NOLINT
  1647, 1073743473, 1747, 1749, 1073743589, 1766, 1073743598, 1775,  // NOLINT
  1073743610, 1788, 1791, 1808, 1073743634, 1839, 1073743693, 1901,  // NOLINT
  1073743744, 1957, 1969, 1073743818, 2026, 1073743860, 2037, 2042,  // NOLINT
  1073744132, 2361, 2365, 2384, 1073744216, 2401, 1073744251, 2431,  // NOLINT
  1073744261, 2444, 1073744271, 2448, 1073744275, 2472, 1073744298, 2480,  // NOLINT
  2482, 1073744310, 2489, 2493, 2510, 1073744348, 2525, 1073744351,  // NOLINT
  2529, 1073744368, 2545, 1073744389, 2570, 1073744399, 2576, 1073744403,  // NOLINT
  2600, 1073744426, 2608, 1073744434, 2611, 1073744437, 2614, 1073744440,  // NOLINT
  2617, 1073744473, 2652, 2654, 1073744498, 2676, 1073744517, 2701,  // NOLINT
  1073744527, 2705, 1073744531, 2728, 1073744554, 2736, 1073744562, 2739,  // NOLINT
  1073744565, 2745, 2749, 2768, 1073744608, 2785, 1073744645, 2828,  // NOLINT
  1073744655, 2832, 1073744659, 2856, 1073744682, 2864, 1073744690, 2867,  // NOLINT
  1073744693, 2873, 2877, 1073744732, 2909, 1073744735, 2913, 2929,  // NOLINT
  2947, 1073744773, 2954, 1073744782, 2960, 1073744786, 2965, 1073744793,  // NOLINT
  2970, 2972, 1073744798, 2975, 1073744803, 2980, 1073744808, 2986,  // NOLINT
  1073744814, 3001, 1073744901, 3084, 1073744910, 3088, 1073744914, 3112,  // NOLINT
  1073744938, 3123, 1073744949, 3129, 1073744992, 3169, 1073745029, 3212,  // NOLINT
  1073745038, 3216, 1073745042, 3240, 1073745066, 3251, 1073745077, 3257,  // NOLINT
  3261, 3294, 1073745120, 3297, 1073745157, 3340, 1073745166, 3344,  // NOLINT
  1073745170, 3368, 1073745194, 3385, 1073745248, 3425, 1073745285, 3478,  // NOLINT
  1073745306, 3505, 1073745331, 3515, 3517, 1073745344, 3526, 1073745409,  // NOLINT
  3632, 1073745458, 3635, 1073745472, 3654, 1073745537, 3714, 3716,  // NOLINT
  1073745543, 3720, 3722, 3725, 1073745556, 3735, 1073745561, 3743,  // NOLINT
  1073745569, 3747, 3749, 3751, 1073745578, 3755, 1073745581, 3760,  // NOLINT
  1073745586, 3763, 3773, 1073745600, 3780, 3782, 1073745628, 3805,  // NOLINT
  3840, 1073745728, 3911, 1073745737, 3946, 1073745800, 3979, 1073745920,  // NOLINT
  4129, 1073745955, 4135, 1073745961, 4138, 1073746000, 4181, 1073746080,  // NOLINT
  4293, 1073746128, 4346, 4348, 1073746176, 4441, 1073746271, 4514,  // NOLINT
  1073746344, 4601, 1073746432, 4680, 1073746506, 4685, 1073746512, 4694,  // NOLINT
  4696, 1073746522, 4701, 1073746528, 4744, 1073746570, 4749, 1073746576,  // NOLINT
  4784, 1073746610, 4789, 1073746616, 4798, 4800, 1073746626, 4805,  // NOLINT
  1073746632, 4822, 1073746648, 4880, 1073746706, 4885, 1073746712, 4954,  // NOLINT
  1073746816, 5007, 1073746848, 5108, 1073746945, 5740, 1073747567, 5750,  // NOLINT
  1073747585, 5786, 1073747616, 5866, 1073747694, 5872, 1073747712, 5900,  // NOLINT
  1073747726, 5905, 1073747744, 5937, 1073747776, 5969, 1073747808, 5996,  // NOLINT
  1073747822, 6000, 1073747840, 6067, 6103, 6108, 1073748000, 6263,  // NOLINT
  1073748096, 6312, 1073748224, 6428, 1073748304, 6509, 1073748336, 6516,  // NOLINT
  1073748352, 6569, 1073748417, 6599, 1073748480, 6678, 1073748741, 6963,  // NOLINT
  1073748805, 6987, 1073749248, 7615, 1073749504, 7835, 1073749664, 7929,  // NOLINT
  1073749760, 7957, 1073749784, 7965, 1073749792, 8005, 1073749832, 8013,  // NOLINT
  1073749840, 8023, 8025, 8027, 8029, 1073749855, 8061, 1073749888,  // NOLINT
  8116, 1073749942, 8124, 8126, 1073749954, 8132, 1073749958, 8140,  // NOLINT
  1073749968, 8147, 1073749974, 8155, 1073749984, 8172, 1073750002, 8180,  // NOLINT
  1073750006, 8188 };  // NOLINT
static const uint16_t kLetterTable1Size = 84;
static const int32_t kLetterTable1[84] = {
  113, 127, 1073741968, 148, 258, 263, 1073742090, 275,  // NOLINT
  277, 1073742105, 285, 292, 294, 296, 1073742122, 301,  // NOLINT
  1073742127, 313, 1073742140, 319, 1073742149, 329, 334, 1073742176,  // NOLINT
  388, 1073744896, 3118, 1073744944, 3166, 1073744992, 3180, 1073745012,  // NOLINT
  3191, 1073745024, 3300, 1073745152, 3365, 1073745200, 3429, 3439,  // NOLINT
  1073745280, 3478, 1073745312, 3494, 1073745320, 3502, 1073745328, 3510,  // NOLINT
  1073745336, 3518, 1073745344, 3526, 1073745352, 3534, 1073745360, 3542,  // NOLINT
  1073745368, 3550, 1073745925, 4103, 1073745953, 4137, 1073745969, 4149,  // NOLINT
  1073745976, 4156, 1073745985, 4246, 1073746077, 4255, 1073746081, 4346,  // NOLINT
  1073746172, 4351, 1073746181, 4396, 1073746225, 4494, 1073746336, 4535,  // NOLINT
  1073746416, 4607, 1073746944, 8191 };  // NOLINT
static const uint16_t kLetterTable2Size = 4;
static const int32_t kLetterTable2[4] = {
  1073741824, 3509, 1073745408, 8191 };  // NOLINT
static const uint16_t kLetterTable3Size = 2;
static const int32_t kLetterTable3[2] = {
  1073741824, 8191 };  // NOLINT
static const uint16_t kLetterTable4Size = 2;
static const int32_t kLetterTable4[2] = {
  1073741824, 8123 };  // NOLINT
static const uint16_t kLetterTable5Size = 16;
static const int32_t kLetterTable5[16] = {
  1073741824, 1164, 1073743639, 1818, 1073743872, 2049, 1073743875, 2053,  // NOLINT
  1073743879, 2058, 1073743884, 2082, 1073743936, 2163, 1073744896, 8191 };  // NOLINT
static const uint16_t kLetterTable6Size = 2;
static const int32_t kLetterTable6[2] = {
  1073741824, 6051 };  // NOLINT
static const uint16_t kLetterTable7Size = 50;
static const int32_t kLetterTable7[50] = {
  1073748224, 6701, 1073748528, 6762, 1073748592, 6873, 1073748736, 6918,  // NOLINT
  1073748755, 6935, 6941, 1073748767, 6952, 1073748778, 6966, 1073748792,  // NOLINT
  6972, 6974, 1073748800, 6977, 1073748803, 6980, 1073748806, 7089,  // NOLINT
  1073748947, 7485, 1073749328, 7567, 1073749394, 7623, 1073749488, 7675,  // NOLINT
  1073749616, 7796, 1073749622, 7932, 1073749793, 7994, 1073749825, 8026,  // NOLINT
  1073749862, 8126, 1073749954, 8135, 1073749962, 8143, 1073749970, 8151,  // NOLINT
  1073749978, 8156 };  // NOLINT
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

static const uint16_t kNumberTable0Size = 44;
static const int32_t kNumberTable0[44] = {
  1073741872, 57, 1073743456, 1641, 1073743600, 1785, 1073743808, 1993,  // NOLINT
  1073744230, 2415, 1073744358, 2543, 1073744486, 2671, 1073744614, 2799,  // NOLINT
  1073744742, 2927, 1073744870, 3055, 1073744998, 3183, 1073745126, 3311,  // NOLINT
  1073745254, 3439, 1073745488, 3673, 1073745616, 3801, 1073745696, 3881,  // NOLINT
  1073745984, 4169, 1073747936, 6121, 1073747984, 6169, 1073748294, 6479,  // NOLINT
  1073748432, 6617, 1073748816, 7001 };  // NOLINT
static const uint16_t kNumberTable7Size = 2;
static const int32_t kNumberTable7[2] = {
  1073749776, 7961 };  // NOLINT
bool Number::Is(uchar c) {
  int chunk_index = c >> 13;
  switch (chunk_index) {
    case 0: return LookupPredicate(kNumberTable0,
                                       kNumberTable0Size,
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

static const uint16_t kCombiningMarkTable0Size = 205;
static const int32_t kCombiningMarkTable0[205] = {
  1073742592, 879, 1073742979, 1158, 1073743249, 1469, 1471, 1073743297,  // NOLINT
  1474, 1073743300, 1477, 1479, 1073743376, 1557, 1073743435, 1630,  // NOLINT
  1648, 1073743574, 1756, 1073743583, 1764, 1073743591, 1768, 1073743594,  // NOLINT
  1773, 1809, 1073743664, 1866, 1073743782, 1968, 1073743851, 2035,  // NOLINT
  1073744129, 2307, 2364, 1073744190, 2381, 1073744209, 2388, 1073744226,  // NOLINT
  2403, 1073744257, 2435, 2492, 1073744318, 2500, 1073744327, 2504,  // NOLINT
  1073744331, 2509, 2519, 1073744354, 2531, 1073744385, 2563, 2620,  // NOLINT
  1073744446, 2626, 1073744455, 2632, 1073744459, 2637, 1073744496, 2673,  // NOLINT
  1073744513, 2691, 2748, 1073744574, 2757, 1073744583, 2761, 1073744587,  // NOLINT
  2765, 1073744610, 2787, 1073744641, 2819, 2876, 1073744702, 2883,  // NOLINT
  1073744711, 2888, 1073744715, 2893, 1073744726, 2903, 2946, 1073744830,  // NOLINT
  3010, 1073744838, 3016, 1073744842, 3021, 3031, 1073744897, 3075,  // NOLINT
  1073744958, 3140, 1073744966, 3144, 1073744970, 3149, 1073744981, 3158,  // NOLINT
  1073745026, 3203, 3260, 1073745086, 3268, 1073745094, 3272, 1073745098,  // NOLINT
  3277, 1073745109, 3286, 1073745122, 3299, 1073745154, 3331, 1073745214,  // NOLINT
  3395, 1073745222, 3400, 1073745226, 3405, 3415, 1073745282, 3459,  // NOLINT
  3530, 1073745359, 3540, 3542, 1073745368, 3551, 1073745394, 3571,  // NOLINT
  3633, 1073745460, 3642, 1073745479, 3662, 3761, 1073745588, 3769,  // NOLINT
  1073745595, 3772, 1073745608, 3789, 1073745688, 3865, 3893, 3895,  // NOLINT
  3897, 1073745726, 3903, 1073745777, 3972, 1073745798, 3975, 1073745808,  // NOLINT
  3991, 1073745817, 4028, 4038, 1073745964, 4146, 1073745974, 4153,  // NOLINT
  1073746006, 4185, 4959, 1073747730, 5908, 1073747762, 5940, 1073747794,  // NOLINT
  5971, 1073747826, 6003, 1073747894, 6099, 6109, 1073747979, 6157,  // NOLINT
  6313, 1073748256, 6443, 1073748272, 6459, 1073748400, 6592, 1073748424,  // NOLINT
  6601, 1073748503, 6683, 1073748736, 6916, 1073748788, 6980, 1073748843,  // NOLINT
  7027, 1073749440, 7626, 1073749502, 7679 };  // NOLINT
static const uint16_t kCombiningMarkTable1Size = 9;
static const int32_t kCombiningMarkTable1[9] = {
  1073742032, 220, 225, 1073742053, 239, 1073745962, 4143, 1073746073,  // NOLINT
  4250 };  // NOLINT
static const uint16_t kCombiningMarkTable5Size = 5;
static const int32_t kCombiningMarkTable5[5] = {
  2050, 2054, 2059, 1073743907, 2087 };  // NOLINT
static const uint16_t kCombiningMarkTable7Size = 5;
static const int32_t kCombiningMarkTable7[5] = {
  6942, 1073749504, 7695, 1073749536, 7715 };  // NOLINT
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
static const uint16_t kToLowercaseTable0Size = 463;  // NOLINT
static const int32_t kToLowercaseTable0[926] = {
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
  902, 152, 1073742728, 148, 906, 148, 908, 256, 1073742734, 252, 911, 252, 1073742737, 128, 929, 128,  // NOLINT
  931, 6, 1073742756, 128, 939, 128, 984, 4, 986, 4, 988, 4, 990, 4, 992, 4,  // NOLINT
  994, 4, 996, 4, 998, 4, 1000, 4, 1002, 4, 1004, 4, 1006, 4, 1012, -240,  // NOLINT
  1015, 4, 1017, -28, 1018, 4, 1073742845, -520, 1023, -520, 1073742848, 320, 1039, 320, 1073742864, 128,  // NOLINT
  1071, 128, 1120, 4, 1122, 4, 1124, 4, 1126, 4, 1128, 4, 1130, 4, 1132, 4,  // NOLINT
  1134, 4, 1136, 4, 1138, 4, 1140, 4, 1142, 4, 1144, 4, 1146, 4, 1148, 4,  // NOLINT
  1150, 4, 1152, 4, 1162, 4, 1164, 4, 1166, 4, 1168, 4, 1170, 4, 1172, 4,  // NOLINT
  1174, 4, 1176, 4, 1178, 4, 1180, 4, 1182, 4, 1184, 4, 1186, 4, 1188, 4,  // NOLINT
  1190, 4, 1192, 4, 1194, 4, 1196, 4, 1198, 4, 1200, 4, 1202, 4, 1204, 4,  // NOLINT
  1206, 4, 1208, 4, 1210, 4, 1212, 4, 1214, 4, 1216, 60, 1217, 4, 1219, 4,  // NOLINT
  1221, 4, 1223, 4, 1225, 4, 1227, 4, 1229, 4, 1232, 4, 1234, 4, 1236, 4,  // NOLINT
  1238, 4, 1240, 4, 1242, 4, 1244, 4, 1246, 4, 1248, 4, 1250, 4, 1252, 4,  // NOLINT
  1254, 4, 1256, 4, 1258, 4, 1260, 4, 1262, 4, 1264, 4, 1266, 4, 1268, 4,  // NOLINT
  1270, 4, 1272, 4, 1274, 4, 1276, 4, 1278, 4, 1280, 4, 1282, 4, 1284, 4,  // NOLINT
  1286, 4, 1288, 4, 1290, 4, 1292, 4, 1294, 4, 1296, 4, 1298, 4, 1073743153, 192,  // NOLINT
  1366, 192, 1073746080, 29056, 4293, 29056, 7680, 4, 7682, 4, 7684, 4, 7686, 4, 7688, 4,  // NOLINT
  7690, 4, 7692, 4, 7694, 4, 7696, 4, 7698, 4, 7700, 4, 7702, 4, 7704, 4,  // NOLINT
  7706, 4, 7708, 4, 7710, 4, 7712, 4, 7714, 4, 7716, 4, 7718, 4, 7720, 4,  // NOLINT
  7722, 4, 7724, 4, 7726, 4, 7728, 4, 7730, 4, 7732, 4, 7734, 4, 7736, 4,  // NOLINT
  7738, 4, 7740, 4, 7742, 4, 7744, 4, 7746, 4, 7748, 4, 7750, 4, 7752, 4,  // NOLINT
  7754, 4, 7756, 4, 7758, 4, 7760, 4, 7762, 4, 7764, 4, 7766, 4, 7768, 4,  // NOLINT
  7770, 4, 7772, 4, 7774, 4, 7776, 4, 7778, 4, 7780, 4, 7782, 4, 7784, 4,  // NOLINT
  7786, 4, 7788, 4, 7790, 4, 7792, 4, 7794, 4, 7796, 4, 7798, 4, 7800, 4,  // NOLINT
  7802, 4, 7804, 4, 7806, 4, 7808, 4, 7810, 4, 7812, 4, 7814, 4, 7816, 4,  // NOLINT
  7818, 4, 7820, 4, 7822, 4, 7824, 4, 7826, 4, 7828, 4, 7840, 4, 7842, 4,  // NOLINT
  7844, 4, 7846, 4, 7848, 4, 7850, 4, 7852, 4, 7854, 4, 7856, 4, 7858, 4,  // NOLINT
  7860, 4, 7862, 4, 7864, 4, 7866, 4, 7868, 4, 7870, 4, 7872, 4, 7874, 4,  // NOLINT
  7876, 4, 7878, 4, 7880, 4, 7882, 4, 7884, 4, 7886, 4, 7888, 4, 7890, 4,  // NOLINT
  7892, 4, 7894, 4, 7896, 4, 7898, 4, 7900, 4, 7902, 4, 7904, 4, 7906, 4,  // NOLINT
  7908, 4, 7910, 4, 7912, 4, 7914, 4, 7916, 4, 7918, 4, 7920, 4, 7922, 4,  // NOLINT
  7924, 4, 7926, 4, 7928, 4, 1073749768, -32, 7951, -32, 1073749784, -32, 7965, -32, 1073749800, -32,  // NOLINT
  7983, -32, 1073749816, -32, 7999, -32, 1073749832, -32, 8013, -32, 8025, -32, 8027, -32, 8029, -32,  // NOLINT
  8031, -32, 1073749864, -32, 8047, -32, 1073749896, -32, 8079, -32, 1073749912, -32, 8095, -32, 1073749928, -32,  // NOLINT
  8111, -32, 1073749944, -32, 8121, -32, 1073749946, -296, 8123, -296, 8124, -36, 1073749960, -344, 8139, -344,  // NOLINT
  8140, -36, 1073749976, -32, 8153, -32, 1073749978, -400, 8155, -400, 1073749992, -32, 8169, -32, 1073749994, -448,  // NOLINT
  8171, -448, 8172, -28, 1073750008, -512, 8185, -512, 1073750010, -504, 8187, -504, 8188, -36 };  // NOLINT
static const uint16_t kToLowercaseMultiStrings0Size = 2;  // NOLINT
static const MultiCharacterSpecialCase<1> kToLowercaseMultiStrings1[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kToLowercaseTable1Size = 69;  // NOLINT
static const int32_t kToLowercaseTable1[138] = {
  294, -30068, 298, -33532, 299, -33048, 306, 112, 1073742176, 64, 367, 64, 387, 4, 1073743030, 104,  // NOLINT
  1231, 104, 1073744896, 192, 3118, 192, 3168, 4, 3170, -42972, 3171, -15256, 3172, -42908, 3175, 4,  // NOLINT
  3177, 4, 3179, 4, 3189, 4, 3200, 4, 3202, 4, 3204, 4, 3206, 4, 3208, 4,  // NOLINT
  3210, 4, 3212, 4, 3214, 4, 3216, 4, 3218, 4, 3220, 4, 3222, 4, 3224, 4,  // NOLINT
  3226, 4, 3228, 4, 3230, 4, 3232, 4, 3234, 4, 3236, 4, 3238, 4, 3240, 4,  // NOLINT
  3242, 4, 3244, 4, 3246, 4, 3248, 4, 3250, 4, 3252, 4, 3254, 4, 3256, 4,  // NOLINT
  3258, 4, 3260, 4, 3262, 4, 3264, 4, 3266, 4, 3268, 4, 3270, 4, 3272, 4,  // NOLINT
  3274, 4, 3276, 4, 3278, 4, 3280, 4, 3282, 4, 3284, 4, 3286, 4, 3288, 4,  // NOLINT
  3290, 4, 3292, 4, 3294, 4, 3296, 4, 3298, 4 };  // NOLINT
static const uint16_t kToLowercaseMultiStrings1Size = 1;  // NOLINT
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
static const uint16_t kToUppercaseTable0Size = 554;  // NOLINT
static const int32_t kToUppercaseTable0[1108] = {
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
  578, -4, 583, -4, 585, -4, 587, -4, 589, -4, 591, -4, 595, -840, 596, -824,  // NOLINT
  1073742422, -820, 599, -820, 601, -808, 603, -812, 608, -820, 611, -828, 616, -836, 617, -844,  // NOLINT
  619, 42972, 623, -844, 626, -852, 629, -856, 637, 42908, 640, -872, 643, -872, 648, -872,  // NOLINT
  649, -276, 1073742474, -868, 651, -868, 652, -284, 658, -876, 837, 336, 1073742715, 520, 893, 520,  // NOLINT
  912, 13, 940, -152, 1073742765, -148, 943, -148, 944, 17, 1073742769, -128, 961, -128, 962, -124,  // NOLINT
  1073742787, -128, 971, -128, 972, -256, 1073742797, -252, 974, -252, 976, -248, 977, -228, 981, -188,  // NOLINT
  982, -216, 985, -4, 987, -4, 989, -4, 991, -4, 993, -4, 995, -4, 997, -4,  // NOLINT
  999, -4, 1001, -4, 1003, -4, 1005, -4, 1007, -4, 1008, -344, 1009, -320, 1010, 28,  // NOLINT
  1013, -384, 1016, -4, 1019, -4, 1073742896, -128, 1103, -128, 1073742928, -320, 1119, -320, 1121, -4,  // NOLINT
  1123, -4, 1125, -4, 1127, -4, 1129, -4, 1131, -4, 1133, -4, 1135, -4, 1137, -4,  // NOLINT
  1139, -4, 1141, -4, 1143, -4, 1145, -4, 1147, -4, 1149, -4, 1151, -4, 1153, -4,  // NOLINT
  1163, -4, 1165, -4, 1167, -4, 1169, -4, 1171, -4, 1173, -4, 1175, -4, 1177, -4,  // NOLINT
  1179, -4, 1181, -4, 1183, -4, 1185, -4, 1187, -4, 1189, -4, 1191, -4, 1193, -4,  // NOLINT
  1195, -4, 1197, -4, 1199, -4, 1201, -4, 1203, -4, 1205, -4, 1207, -4, 1209, -4,  // NOLINT
  1211, -4, 1213, -4, 1215, -4, 1218, -4, 1220, -4, 1222, -4, 1224, -4, 1226, -4,  // NOLINT
  1228, -4, 1230, -4, 1231, -60, 1233, -4, 1235, -4, 1237, -4, 1239, -4, 1241, -4,  // NOLINT
  1243, -4, 1245, -4, 1247, -4, 1249, -4, 1251, -4, 1253, -4, 1255, -4, 1257, -4,  // NOLINT
  1259, -4, 1261, -4, 1263, -4, 1265, -4, 1267, -4, 1269, -4, 1271, -4, 1273, -4,  // NOLINT
  1275, -4, 1277, -4, 1279, -4, 1281, -4, 1283, -4, 1285, -4, 1287, -4, 1289, -4,  // NOLINT
  1291, -4, 1293, -4, 1295, -4, 1297, -4, 1299, -4, 1073743201, -192, 1414, -192, 1415, 21,  // NOLINT
  7549, 15256, 7681, -4, 7683, -4, 7685, -4, 7687, -4, 7689, -4, 7691, -4, 7693, -4,  // NOLINT
  7695, -4, 7697, -4, 7699, -4, 7701, -4, 7703, -4, 7705, -4, 7707, -4, 7709, -4,  // NOLINT
  7711, -4, 7713, -4, 7715, -4, 7717, -4, 7719, -4, 7721, -4, 7723, -4, 7725, -4,  // NOLINT
  7727, -4, 7729, -4, 7731, -4, 7733, -4, 7735, -4, 7737, -4, 7739, -4, 7741, -4,  // NOLINT
  7743, -4, 7745, -4, 7747, -4, 7749, -4, 7751, -4, 7753, -4, 7755, -4, 7757, -4,  // NOLINT
  7759, -4, 7761, -4, 7763, -4, 7765, -4, 7767, -4, 7769, -4, 7771, -4, 7773, -4,  // NOLINT
  7775, -4, 7777, -4, 7779, -4, 7781, -4, 7783, -4, 7785, -4, 7787, -4, 7789, -4,  // NOLINT
  7791, -4, 7793, -4, 7795, -4, 7797, -4, 7799, -4, 7801, -4, 7803, -4, 7805, -4,  // NOLINT
  7807, -4, 7809, -4, 7811, -4, 7813, -4, 7815, -4, 7817, -4, 7819, -4, 7821, -4,  // NOLINT
  7823, -4, 7825, -4, 7827, -4, 7829, -4, 7830, 25, 7831, 29, 7832, 33, 7833, 37,  // NOLINT
  7834, 41, 7835, -236, 7841, -4, 7843, -4, 7845, -4, 7847, -4, 7849, -4, 7851, -4,  // NOLINT
  7853, -4, 7855, -4, 7857, -4, 7859, -4, 7861, -4, 7863, -4, 7865, -4, 7867, -4,  // NOLINT
  7869, -4, 7871, -4, 7873, -4, 7875, -4, 7877, -4, 7879, -4, 7881, -4, 7883, -4,  // NOLINT
  7885, -4, 7887, -4, 7889, -4, 7891, -4, 7893, -4, 7895, -4, 7897, -4, 7899, -4,  // NOLINT
  7901, -4, 7903, -4, 7905, -4, 7907, -4, 7909, -4, 7911, -4, 7913, -4, 7915, -4,  // NOLINT
  7917, -4, 7919, -4, 7921, -4, 7923, -4, 7925, -4, 7927, -4, 7929, -4, 1073749760, 32,  // NOLINT
  7943, 32, 1073749776, 32, 7957, 32, 1073749792, 32, 7975, 32, 1073749808, 32, 7991, 32, 1073749824, 32,  // NOLINT
  8005, 32, 8016, 45, 8017, 32, 8018, 49, 8019, 32, 8020, 53, 8021, 32, 8022, 57,  // NOLINT
  8023, 32, 1073749856, 32, 8039, 32, 1073749872, 296, 8049, 296, 1073749874, 344, 8053, 344, 1073749878, 400,  // NOLINT
  8055, 400, 1073749880, 512, 8057, 512, 1073749882, 448, 8059, 448, 1073749884, 504, 8061, 504, 8064, 61,  // NOLINT
  8065, 65, 8066, 69, 8067, 73, 8068, 77, 8069, 81, 8070, 85, 8071, 89, 8072, 61,  // NOLINT
  8073, 65, 8074, 69, 8075, 73, 8076, 77, 8077, 81, 8078, 85, 8079, 89, 8080, 93,  // NOLINT
  8081, 97, 8082, 101, 8083, 105, 8084, 109, 8085, 113, 8086, 117, 8087, 121, 8088, 93,  // NOLINT
  8089, 97, 8090, 101, 8091, 105, 8092, 109, 8093, 113, 8094, 117, 8095, 121, 8096, 125,  // NOLINT
  8097, 129, 8098, 133, 8099, 137, 8100, 141, 8101, 145, 8102, 149, 8103, 153, 8104, 125,  // NOLINT
  8105, 129, 8106, 133, 8107, 137, 8108, 141, 8109, 145, 8110, 149, 8111, 153, 1073749936, 32,  // NOLINT
  8113, 32, 8114, 157, 8115, 161, 8116, 165, 8118, 169, 8119, 173, 8124, 161, 8126, -28820,  // NOLINT
  8130, 177, 8131, 181, 8132, 185, 8134, 189, 8135, 193, 8140, 181, 1073749968, 32, 8145, 32,  // NOLINT
  8146, 197, 8147, 13, 8150, 201, 8151, 205, 1073749984, 32, 8161, 32, 8162, 209, 8163, 17,  // NOLINT
  8164, 213, 8165, 28, 8166, 217, 8167, 221, 8178, 225, 8179, 229, 8180, 233, 8182, 237,  // NOLINT
  8183, 241, 8188, 229 };  // NOLINT
static const uint16_t kToUppercaseMultiStrings0Size = 62;  // NOLINT
static const MultiCharacterSpecialCase<1> kToUppercaseMultiStrings1[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kToUppercaseTable1Size = 67;  // NOLINT
static const int32_t kToUppercaseTable1[134] = {
  334, -112, 1073742192, -64, 383, -64, 388, -4, 1073743056, -104, 1257, -104, 1073744944, -192, 3166, -192,  // NOLINT
  3169, -4, 3173, -43180, 3174, -43168, 3176, -4, 3178, -4, 3180, -4, 3190, -4, 3201, -4,  // NOLINT
  3203, -4, 3205, -4, 3207, -4, 3209, -4, 3211, -4, 3213, -4, 3215, -4, 3217, -4,  // NOLINT
  3219, -4, 3221, -4, 3223, -4, 3225, -4, 3227, -4, 3229, -4, 3231, -4, 3233, -4,  // NOLINT
  3235, -4, 3237, -4, 3239, -4, 3241, -4, 3243, -4, 3245, -4, 3247, -4, 3249, -4,  // NOLINT
  3251, -4, 3253, -4, 3255, -4, 3257, -4, 3259, -4, 3261, -4, 3263, -4, 3265, -4,  // NOLINT
  3267, -4, 3269, -4, 3271, -4, 3273, -4, 3275, -4, 3277, -4, 3279, -4, 3281, -4,  // NOLINT
  3283, -4, 3285, -4, 3287, -4, 3289, -4, 3291, -4, 3293, -4, 3295, -4, 3297, -4,  // NOLINT
  3299, -4, 1073745152, -29056, 3365, -29056 };  // NOLINT
static const uint16_t kToUppercaseMultiStrings1Size = 1;  // NOLINT
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
static const uint16_t kEcma262CanonicalizeTable0Size = 462;  // NOLINT
static const int32_t kEcma262CanonicalizeTable0[924] = {
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
  561, -4, 563, -4, 572, -4, 578, -4, 583, -4, 585, -4, 587, -4, 589, -4,  // NOLINT
  591, -4, 595, -840, 596, -824, 1073742422, -820, 599, -820, 601, -808, 603, -812, 608, -820,  // NOLINT
  611, -828, 616, -836, 617, -844, 619, 42972, 623, -844, 626, -852, 629, -856, 637, 42908,  // NOLINT
  640, -872, 643, -872, 648, -872, 649, -276, 1073742474, -868, 651, -868, 652, -284, 658, -876,  // NOLINT
  837, 336, 1073742715, 520, 893, 520, 940, -152, 1073742765, -148, 943, -148, 1073742769, -128, 961, -128,  // NOLINT
  962, -124, 1073742787, -128, 971, -128, 972, -256, 1073742797, -252, 974, -252, 976, -248, 977, -228,  // NOLINT
  981, -188, 982, -216, 985, -4, 987, -4, 989, -4, 991, -4, 993, -4, 995, -4,  // NOLINT
  997, -4, 999, -4, 1001, -4, 1003, -4, 1005, -4, 1007, -4, 1008, -344, 1009, -320,  // NOLINT
  1010, 28, 1013, -384, 1016, -4, 1019, -4, 1073742896, -128, 1103, -128, 1073742928, -320, 1119, -320,  // NOLINT
  1121, -4, 1123, -4, 1125, -4, 1127, -4, 1129, -4, 1131, -4, 1133, -4, 1135, -4,  // NOLINT
  1137, -4, 1139, -4, 1141, -4, 1143, -4, 1145, -4, 1147, -4, 1149, -4, 1151, -4,  // NOLINT
  1153, -4, 1163, -4, 1165, -4, 1167, -4, 1169, -4, 1171, -4, 1173, -4, 1175, -4,  // NOLINT
  1177, -4, 1179, -4, 1181, -4, 1183, -4, 1185, -4, 1187, -4, 1189, -4, 1191, -4,  // NOLINT
  1193, -4, 1195, -4, 1197, -4, 1199, -4, 1201, -4, 1203, -4, 1205, -4, 1207, -4,  // NOLINT
  1209, -4, 1211, -4, 1213, -4, 1215, -4, 1218, -4, 1220, -4, 1222, -4, 1224, -4,  // NOLINT
  1226, -4, 1228, -4, 1230, -4, 1231, -60, 1233, -4, 1235, -4, 1237, -4, 1239, -4,  // NOLINT
  1241, -4, 1243, -4, 1245, -4, 1247, -4, 1249, -4, 1251, -4, 1253, -4, 1255, -4,  // NOLINT
  1257, -4, 1259, -4, 1261, -4, 1263, -4, 1265, -4, 1267, -4, 1269, -4, 1271, -4,  // NOLINT
  1273, -4, 1275, -4, 1277, -4, 1279, -4, 1281, -4, 1283, -4, 1285, -4, 1287, -4,  // NOLINT
  1289, -4, 1291, -4, 1293, -4, 1295, -4, 1297, -4, 1299, -4, 1073743201, -192, 1414, -192,  // NOLINT
  7549, 15256, 7681, -4, 7683, -4, 7685, -4, 7687, -4, 7689, -4, 7691, -4, 7693, -4,  // NOLINT
  7695, -4, 7697, -4, 7699, -4, 7701, -4, 7703, -4, 7705, -4, 7707, -4, 7709, -4,  // NOLINT
  7711, -4, 7713, -4, 7715, -4, 7717, -4, 7719, -4, 7721, -4, 7723, -4, 7725, -4,  // NOLINT
  7727, -4, 7729, -4, 7731, -4, 7733, -4, 7735, -4, 7737, -4, 7739, -4, 7741, -4,  // NOLINT
  7743, -4, 7745, -4, 7747, -4, 7749, -4, 7751, -4, 7753, -4, 7755, -4, 7757, -4,  // NOLINT
  7759, -4, 7761, -4, 7763, -4, 7765, -4, 7767, -4, 7769, -4, 7771, -4, 7773, -4,  // NOLINT
  7775, -4, 7777, -4, 7779, -4, 7781, -4, 7783, -4, 7785, -4, 7787, -4, 7789, -4,  // NOLINT
  7791, -4, 7793, -4, 7795, -4, 7797, -4, 7799, -4, 7801, -4, 7803, -4, 7805, -4,  // NOLINT
  7807, -4, 7809, -4, 7811, -4, 7813, -4, 7815, -4, 7817, -4, 7819, -4, 7821, -4,  // NOLINT
  7823, -4, 7825, -4, 7827, -4, 7829, -4, 7835, -236, 7841, -4, 7843, -4, 7845, -4,  // NOLINT
  7847, -4, 7849, -4, 7851, -4, 7853, -4, 7855, -4, 7857, -4, 7859, -4, 7861, -4,  // NOLINT
  7863, -4, 7865, -4, 7867, -4, 7869, -4, 7871, -4, 7873, -4, 7875, -4, 7877, -4,  // NOLINT
  7879, -4, 7881, -4, 7883, -4, 7885, -4, 7887, -4, 7889, -4, 7891, -4, 7893, -4,  // NOLINT
  7895, -4, 7897, -4, 7899, -4, 7901, -4, 7903, -4, 7905, -4, 7907, -4, 7909, -4,  // NOLINT
  7911, -4, 7913, -4, 7915, -4, 7917, -4, 7919, -4, 7921, -4, 7923, -4, 7925, -4,  // NOLINT
  7927, -4, 7929, -4, 1073749760, 32, 7943, 32, 1073749776, 32, 7957, 32, 1073749792, 32, 7975, 32,  // NOLINT
  1073749808, 32, 7991, 32, 1073749824, 32, 8005, 32, 8017, 32, 8019, 32, 8021, 32, 8023, 32,  // NOLINT
  1073749856, 32, 8039, 32, 1073749872, 296, 8049, 296, 1073749874, 344, 8053, 344, 1073749878, 400, 8055, 400,  // NOLINT
  1073749880, 512, 8057, 512, 1073749882, 448, 8059, 448, 1073749884, 504, 8061, 504, 1073749936, 32, 8113, 32,  // NOLINT
  8126, -28820, 1073749968, 32, 8145, 32, 1073749984, 32, 8161, 32, 8165, 28 };  // NOLINT
static const uint16_t kEcma262CanonicalizeMultiStrings0Size = 1;  // NOLINT
static const MultiCharacterSpecialCase<1> kEcma262CanonicalizeMultiStrings1[1] = {  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262CanonicalizeTable1Size = 67;  // NOLINT
static const int32_t kEcma262CanonicalizeTable1[134] = {
  334, -112, 1073742192, -64, 383, -64, 388, -4, 1073743056, -104, 1257, -104, 1073744944, -192, 3166, -192,  // NOLINT
  3169, -4, 3173, -43180, 3174, -43168, 3176, -4, 3178, -4, 3180, -4, 3190, -4, 3201, -4,  // NOLINT
  3203, -4, 3205, -4, 3207, -4, 3209, -4, 3211, -4, 3213, -4, 3215, -4, 3217, -4,  // NOLINT
  3219, -4, 3221, -4, 3223, -4, 3225, -4, 3227, -4, 3229, -4, 3231, -4, 3233, -4,  // NOLINT
  3235, -4, 3237, -4, 3239, -4, 3241, -4, 3243, -4, 3245, -4, 3247, -4, 3249, -4,  // NOLINT
  3251, -4, 3253, -4, 3255, -4, 3257, -4, 3259, -4, 3261, -4, 3263, -4, 3265, -4,  // NOLINT
  3267, -4, 3269, -4, 3271, -4, 3273, -4, 3275, -4, 3277, -4, 3279, -4, 3281, -4,  // NOLINT
  3283, -4, 3285, -4, 3287, -4, 3289, -4, 3291, -4, 3293, -4, 3295, -4, 3297, -4,  // NOLINT
  3299, -4, 1073745152, -29056, 3365, -29056 };  // NOLINT
static const uint16_t kEcma262CanonicalizeMultiStrings1Size = 1;  // NOLINT
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

static const MultiCharacterSpecialCase<4> kEcma262UnCanonicalizeMultiStrings0[469] = {  // NOLINT
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
  {{577, 578, kSentinel}}, {{580, 649, kSentinel}}, {{581, 652, kSentinel}}, {{582, 583, kSentinel}},  // NOLINT
  {{584, 585, kSentinel}}, {{586, 587, kSentinel}}, {{588, 589, kSentinel}}, {{590, 591, kSentinel}},  // NOLINT
  {{619, 11362, kSentinel}}, {{637, 11364, kSentinel}}, {{837, 921, 953, 8126}}, {{891, 1021, kSentinel}},  // NOLINT
  {{893, 1023, kSentinel}}, {{902, 940, kSentinel}}, {{904, 941, kSentinel}}, {{906, 943, kSentinel}},  // NOLINT
  {{908, 972, kSentinel}}, {{910, 973, kSentinel}}, {{911, 974, kSentinel}}, {{913, 945, kSentinel}},  // NOLINT
  {{914, 946, 976, kSentinel}}, {{915, 947, kSentinel}}, {{916, 948, kSentinel}}, {{917, 949, 1013, kSentinel}},  // NOLINT
  {{918, 950, kSentinel}}, {{919, 951, kSentinel}}, {{920, 952, 977, kSentinel}}, {{922, 954, 1008, kSentinel}},  // NOLINT
  {{923, 955, kSentinel}}, {{925, 957, kSentinel}}, {{927, 959, kSentinel}}, {{928, 960, 982, kSentinel}},  // NOLINT
  {{929, 961, 1009, kSentinel}}, {{931, 962, 963, kSentinel}}, {{932, 964, kSentinel}}, {{933, 965, kSentinel}},  // NOLINT
  {{934, 966, 981, kSentinel}}, {{935, 967, kSentinel}}, {{939, 971, kSentinel}}, {{984, 985, kSentinel}},  // NOLINT
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
  {{1329, 1377, kSentinel}}, {{1366, 1414, kSentinel}}, {{4256, 11520, kSentinel}}, {{4293, 11557, kSentinel}},  // NOLINT
  {{7549, 11363, kSentinel}}, {{7680, 7681, kSentinel}}, {{7682, 7683, kSentinel}}, {{7684, 7685, kSentinel}},  // NOLINT
  {{7686, 7687, kSentinel}}, {{7688, 7689, kSentinel}}, {{7690, 7691, kSentinel}}, {{7692, 7693, kSentinel}},  // NOLINT
  {{7694, 7695, kSentinel}}, {{7696, 7697, kSentinel}}, {{7698, 7699, kSentinel}}, {{7700, 7701, kSentinel}},  // NOLINT
  {{7702, 7703, kSentinel}}, {{7704, 7705, kSentinel}}, {{7706, 7707, kSentinel}}, {{7708, 7709, kSentinel}},  // NOLINT
  {{7710, 7711, kSentinel}}, {{7712, 7713, kSentinel}}, {{7714, 7715, kSentinel}}, {{7716, 7717, kSentinel}},  // NOLINT
  {{7718, 7719, kSentinel}}, {{7720, 7721, kSentinel}}, {{7722, 7723, kSentinel}}, {{7724, 7725, kSentinel}},  // NOLINT
  {{7726, 7727, kSentinel}}, {{7728, 7729, kSentinel}}, {{7730, 7731, kSentinel}}, {{7732, 7733, kSentinel}},  // NOLINT
  {{7734, 7735, kSentinel}}, {{7736, 7737, kSentinel}}, {{7738, 7739, kSentinel}}, {{7740, 7741, kSentinel}},  // NOLINT
  {{7742, 7743, kSentinel}}, {{7744, 7745, kSentinel}}, {{7746, 7747, kSentinel}}, {{7748, 7749, kSentinel}},  // NOLINT
  {{7750, 7751, kSentinel}}, {{7752, 7753, kSentinel}}, {{7754, 7755, kSentinel}}, {{7756, 7757, kSentinel}},  // NOLINT
  {{7758, 7759, kSentinel}}, {{7760, 7761, kSentinel}}, {{7762, 7763, kSentinel}}, {{7764, 7765, kSentinel}},  // NOLINT
  {{7766, 7767, kSentinel}}, {{7768, 7769, kSentinel}}, {{7770, 7771, kSentinel}}, {{7772, 7773, kSentinel}},  // NOLINT
  {{7774, 7775, kSentinel}}, {{7776, 7777, 7835, kSentinel}}, {{7778, 7779, kSentinel}}, {{7780, 7781, kSentinel}},  // NOLINT
  {{7782, 7783, kSentinel}}, {{7784, 7785, kSentinel}}, {{7786, 7787, kSentinel}}, {{7788, 7789, kSentinel}},  // NOLINT
  {{7790, 7791, kSentinel}}, {{7792, 7793, kSentinel}}, {{7794, 7795, kSentinel}}, {{7796, 7797, kSentinel}},  // NOLINT
  {{7798, 7799, kSentinel}}, {{7800, 7801, kSentinel}}, {{7802, 7803, kSentinel}}, {{7804, 7805, kSentinel}},  // NOLINT
  {{7806, 7807, kSentinel}}, {{7808, 7809, kSentinel}}, {{7810, 7811, kSentinel}}, {{7812, 7813, kSentinel}},  // NOLINT
  {{7814, 7815, kSentinel}}, {{7816, 7817, kSentinel}}, {{7818, 7819, kSentinel}}, {{7820, 7821, kSentinel}},  // NOLINT
  {{7822, 7823, kSentinel}}, {{7824, 7825, kSentinel}}, {{7826, 7827, kSentinel}}, {{7828, 7829, kSentinel}},  // NOLINT
  {{7840, 7841, kSentinel}}, {{7842, 7843, kSentinel}}, {{7844, 7845, kSentinel}}, {{7846, 7847, kSentinel}},  // NOLINT
  {{7848, 7849, kSentinel}}, {{7850, 7851, kSentinel}}, {{7852, 7853, kSentinel}}, {{7854, 7855, kSentinel}},  // NOLINT
  {{7856, 7857, kSentinel}}, {{7858, 7859, kSentinel}}, {{7860, 7861, kSentinel}}, {{7862, 7863, kSentinel}},  // NOLINT
  {{7864, 7865, kSentinel}}, {{7866, 7867, kSentinel}}, {{7868, 7869, kSentinel}}, {{7870, 7871, kSentinel}},  // NOLINT
  {{7872, 7873, kSentinel}}, {{7874, 7875, kSentinel}}, {{7876, 7877, kSentinel}}, {{7878, 7879, kSentinel}},  // NOLINT
  {{7880, 7881, kSentinel}}, {{7882, 7883, kSentinel}}, {{7884, 7885, kSentinel}}, {{7886, 7887, kSentinel}},  // NOLINT
  {{7888, 7889, kSentinel}}, {{7890, 7891, kSentinel}}, {{7892, 7893, kSentinel}}, {{7894, 7895, kSentinel}},  // NOLINT
  {{7896, 7897, kSentinel}}, {{7898, 7899, kSentinel}}, {{7900, 7901, kSentinel}}, {{7902, 7903, kSentinel}},  // NOLINT
  {{7904, 7905, kSentinel}}, {{7906, 7907, kSentinel}}, {{7908, 7909, kSentinel}}, {{7910, 7911, kSentinel}},  // NOLINT
  {{7912, 7913, kSentinel}}, {{7914, 7915, kSentinel}}, {{7916, 7917, kSentinel}}, {{7918, 7919, kSentinel}},  // NOLINT
  {{7920, 7921, kSentinel}}, {{7922, 7923, kSentinel}}, {{7924, 7925, kSentinel}}, {{7926, 7927, kSentinel}},  // NOLINT
  {{7928, 7929, kSentinel}}, {{7936, 7944, kSentinel}}, {{7943, 7951, kSentinel}}, {{7952, 7960, kSentinel}},  // NOLINT
  {{7957, 7965, kSentinel}}, {{7968, 7976, kSentinel}}, {{7975, 7983, kSentinel}}, {{7984, 7992, kSentinel}},  // NOLINT
  {{7991, 7999, kSentinel}}, {{8000, 8008, kSentinel}}, {{8005, 8013, kSentinel}}, {{8017, 8025, kSentinel}},  // NOLINT
  {{8019, 8027, kSentinel}}, {{8021, 8029, kSentinel}}, {{8023, 8031, kSentinel}}, {{8032, 8040, kSentinel}},  // NOLINT
  {{8039, 8047, kSentinel}}, {{8048, 8122, kSentinel}}, {{8049, 8123, kSentinel}}, {{8050, 8136, kSentinel}},  // NOLINT
  {{8053, 8139, kSentinel}}, {{8054, 8154, kSentinel}}, {{8055, 8155, kSentinel}}, {{8056, 8184, kSentinel}},  // NOLINT
  {{8057, 8185, kSentinel}}, {{8058, 8170, kSentinel}}, {{8059, 8171, kSentinel}}, {{8060, 8186, kSentinel}},  // NOLINT
  {{8061, 8187, kSentinel}}, {{8112, 8120, kSentinel}}, {{8113, 8121, kSentinel}}, {{8144, 8152, kSentinel}},  // NOLINT
  {{8145, 8153, kSentinel}}, {{8160, 8168, kSentinel}}, {{8161, 8169, kSentinel}}, {{8165, 8172, kSentinel}},  // NOLINT
  {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262UnCanonicalizeTable0Size = 945;  // NOLINT
static const int32_t kEcma262UnCanonicalizeTable0[1890] = {
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
  570, 645, 571, 649, 572, 649, 573, 353, 574, 653, 577, 657, 578, 657, 579, 277,  // NOLINT
  580, 661, 581, 665, 582, 669, 583, 669, 584, 673, 585, 673, 586, 677, 587, 677,  // NOLINT
  588, 681, 589, 681, 590, 685, 591, 685, 595, 281, 596, 293, 1073742422, 301, 599, 305,  // NOLINT
  601, 317, 603, 321, 608, 329, 611, 333, 616, 345, 617, 341, 619, 689, 623, 357,  // NOLINT
  626, 361, 629, 369, 637, 693, 640, 385, 643, 393, 648, 401, 649, 661, 1073742474, 409,  // NOLINT
  651, 413, 652, 665, 658, 425, 837, 697, 1073742715, 701, 893, 705, 902, 709, 1073742728, 713,  // NOLINT
  906, 717, 908, 721, 1073742734, 725, 911, 729, 913, 733, 914, 737, 1073742739, 741, 916, 745,  // NOLINT
  917, 749, 1073742742, 753, 919, 757, 920, 761, 921, 697, 922, 765, 923, 769, 924, 9,  // NOLINT
  1073742749, 773, 927, 777, 928, 781, 929, 785, 931, 789, 1073742756, 793, 933, 797, 934, 801,  // NOLINT
  1073742759, 805, 939, 809, 940, 709, 1073742765, 713, 943, 717, 945, 733, 946, 737, 1073742771, 741,  // NOLINT
  948, 745, 949, 749, 1073742774, 753, 951, 757, 952, 761, 953, 697, 954, 765, 955, 769,  // NOLINT
  956, 9, 1073742781, 773, 959, 777, 960, 781, 961, 785, 962, 789, 963, 789, 1073742788, 793,  // NOLINT
  965, 797, 966, 801, 1073742791, 805, 971, 809, 972, 721, 1073742797, 725, 974, 729, 976, 737,  // NOLINT
  977, 761, 981, 801, 982, 781, 984, 813, 985, 813, 986, 817, 987, 817, 988, 821,  // NOLINT
  989, 821, 990, 825, 991, 825, 992, 829, 993, 829, 994, 833, 995, 833, 996, 837,  // NOLINT
  997, 837, 998, 841, 999, 841, 1000, 845, 1001, 845, 1002, 849, 1003, 849, 1004, 853,  // NOLINT
  1005, 853, 1006, 857, 1007, 857, 1008, 765, 1009, 785, 1010, 861, 1013, 749, 1015, 865,  // NOLINT
  1016, 865, 1017, 861, 1018, 869, 1019, 869, 1073742845, 701, 1023, 705, 1073742848, 873, 1039, 877,  // NOLINT
  1073742864, 881, 1071, 885, 1073742896, 881, 1103, 885, 1073742928, 873, 1119, 877, 1120, 889, 1121, 889,  // NOLINT
  1122, 893, 1123, 893, 1124, 897, 1125, 897, 1126, 901, 1127, 901, 1128, 905, 1129, 905,  // NOLINT
  1130, 909, 1131, 909, 1132, 913, 1133, 913, 1134, 917, 1135, 917, 1136, 921, 1137, 921,  // NOLINT
  1138, 925, 1139, 925, 1140, 929, 1141, 929, 1142, 933, 1143, 933, 1144, 937, 1145, 937,  // NOLINT
  1146, 941, 1147, 941, 1148, 945, 1149, 945, 1150, 949, 1151, 949, 1152, 953, 1153, 953,  // NOLINT
  1162, 957, 1163, 957, 1164, 961, 1165, 961, 1166, 965, 1167, 965, 1168, 969, 1169, 969,  // NOLINT
  1170, 973, 1171, 973, 1172, 977, 1173, 977, 1174, 981, 1175, 981, 1176, 985, 1177, 985,  // NOLINT
  1178, 989, 1179, 989, 1180, 993, 1181, 993, 1182, 997, 1183, 997, 1184, 1001, 1185, 1001,  // NOLINT
  1186, 1005, 1187, 1005, 1188, 1009, 1189, 1009, 1190, 1013, 1191, 1013, 1192, 1017, 1193, 1017,  // NOLINT
  1194, 1021, 1195, 1021, 1196, 1025, 1197, 1025, 1198, 1029, 1199, 1029, 1200, 1033, 1201, 1033,  // NOLINT
  1202, 1037, 1203, 1037, 1204, 1041, 1205, 1041, 1206, 1045, 1207, 1045, 1208, 1049, 1209, 1049,  // NOLINT
  1210, 1053, 1211, 1053, 1212, 1057, 1213, 1057, 1214, 1061, 1215, 1061, 1216, 1065, 1217, 1069,  // NOLINT
  1218, 1069, 1219, 1073, 1220, 1073, 1221, 1077, 1222, 1077, 1223, 1081, 1224, 1081, 1225, 1085,  // NOLINT
  1226, 1085, 1227, 1089, 1228, 1089, 1229, 1093, 1230, 1093, 1231, 1065, 1232, 1097, 1233, 1097,  // NOLINT
  1234, 1101, 1235, 1101, 1236, 1105, 1237, 1105, 1238, 1109, 1239, 1109, 1240, 1113, 1241, 1113,  // NOLINT
  1242, 1117, 1243, 1117, 1244, 1121, 1245, 1121, 1246, 1125, 1247, 1125, 1248, 1129, 1249, 1129,  // NOLINT
  1250, 1133, 1251, 1133, 1252, 1137, 1253, 1137, 1254, 1141, 1255, 1141, 1256, 1145, 1257, 1145,  // NOLINT
  1258, 1149, 1259, 1149, 1260, 1153, 1261, 1153, 1262, 1157, 1263, 1157, 1264, 1161, 1265, 1161,  // NOLINT
  1266, 1165, 1267, 1165, 1268, 1169, 1269, 1169, 1270, 1173, 1271, 1173, 1272, 1177, 1273, 1177,  // NOLINT
  1274, 1181, 1275, 1181, 1276, 1185, 1277, 1185, 1278, 1189, 1279, 1189, 1280, 1193, 1281, 1193,  // NOLINT
  1282, 1197, 1283, 1197, 1284, 1201, 1285, 1201, 1286, 1205, 1287, 1205, 1288, 1209, 1289, 1209,  // NOLINT
  1290, 1213, 1291, 1213, 1292, 1217, 1293, 1217, 1294, 1221, 1295, 1221, 1296, 1225, 1297, 1225,  // NOLINT
  1298, 1229, 1299, 1229, 1073743153, 1233, 1366, 1237, 1073743201, 1233, 1414, 1237, 1073746080, 1241, 4293, 1245,  // NOLINT
  7549, 1249, 7680, 1253, 7681, 1253, 7682, 1257, 7683, 1257, 7684, 1261, 7685, 1261, 7686, 1265,  // NOLINT
  7687, 1265, 7688, 1269, 7689, 1269, 7690, 1273, 7691, 1273, 7692, 1277, 7693, 1277, 7694, 1281,  // NOLINT
  7695, 1281, 7696, 1285, 7697, 1285, 7698, 1289, 7699, 1289, 7700, 1293, 7701, 1293, 7702, 1297,  // NOLINT
  7703, 1297, 7704, 1301, 7705, 1301, 7706, 1305, 7707, 1305, 7708, 1309, 7709, 1309, 7710, 1313,  // NOLINT
  7711, 1313, 7712, 1317, 7713, 1317, 7714, 1321, 7715, 1321, 7716, 1325, 7717, 1325, 7718, 1329,  // NOLINT
  7719, 1329, 7720, 1333, 7721, 1333, 7722, 1337, 7723, 1337, 7724, 1341, 7725, 1341, 7726, 1345,  // NOLINT
  7727, 1345, 7728, 1349, 7729, 1349, 7730, 1353, 7731, 1353, 7732, 1357, 7733, 1357, 7734, 1361,  // NOLINT
  7735, 1361, 7736, 1365, 7737, 1365, 7738, 1369, 7739, 1369, 7740, 1373, 7741, 1373, 7742, 1377,  // NOLINT
  7743, 1377, 7744, 1381, 7745, 1381, 7746, 1385, 7747, 1385, 7748, 1389, 7749, 1389, 7750, 1393,  // NOLINT
  7751, 1393, 7752, 1397, 7753, 1397, 7754, 1401, 7755, 1401, 7756, 1405, 7757, 1405, 7758, 1409,  // NOLINT
  7759, 1409, 7760, 1413, 7761, 1413, 7762, 1417, 7763, 1417, 7764, 1421, 7765, 1421, 7766, 1425,  // NOLINT
  7767, 1425, 7768, 1429, 7769, 1429, 7770, 1433, 7771, 1433, 7772, 1437, 7773, 1437, 7774, 1441,  // NOLINT
  7775, 1441, 7776, 1445, 7777, 1445, 7778, 1449, 7779, 1449, 7780, 1453, 7781, 1453, 7782, 1457,  // NOLINT
  7783, 1457, 7784, 1461, 7785, 1461, 7786, 1465, 7787, 1465, 7788, 1469, 7789, 1469, 7790, 1473,  // NOLINT
  7791, 1473, 7792, 1477, 7793, 1477, 7794, 1481, 7795, 1481, 7796, 1485, 7797, 1485, 7798, 1489,  // NOLINT
  7799, 1489, 7800, 1493, 7801, 1493, 7802, 1497, 7803, 1497, 7804, 1501, 7805, 1501, 7806, 1505,  // NOLINT
  7807, 1505, 7808, 1509, 7809, 1509, 7810, 1513, 7811, 1513, 7812, 1517, 7813, 1517, 7814, 1521,  // NOLINT
  7815, 1521, 7816, 1525, 7817, 1525, 7818, 1529, 7819, 1529, 7820, 1533, 7821, 1533, 7822, 1537,  // NOLINT
  7823, 1537, 7824, 1541, 7825, 1541, 7826, 1545, 7827, 1545, 7828, 1549, 7829, 1549, 7835, 1445,  // NOLINT
  7840, 1553, 7841, 1553, 7842, 1557, 7843, 1557, 7844, 1561, 7845, 1561, 7846, 1565, 7847, 1565,  // NOLINT
  7848, 1569, 7849, 1569, 7850, 1573, 7851, 1573, 7852, 1577, 7853, 1577, 7854, 1581, 7855, 1581,  // NOLINT
  7856, 1585, 7857, 1585, 7858, 1589, 7859, 1589, 7860, 1593, 7861, 1593, 7862, 1597, 7863, 1597,  // NOLINT
  7864, 1601, 7865, 1601, 7866, 1605, 7867, 1605, 7868, 1609, 7869, 1609, 7870, 1613, 7871, 1613,  // NOLINT
  7872, 1617, 7873, 1617, 7874, 1621, 7875, 1621, 7876, 1625, 7877, 1625, 7878, 1629, 7879, 1629,  // NOLINT
  7880, 1633, 7881, 1633, 7882, 1637, 7883, 1637, 7884, 1641, 7885, 1641, 7886, 1645, 7887, 1645,  // NOLINT
  7888, 1649, 7889, 1649, 7890, 1653, 7891, 1653, 7892, 1657, 7893, 1657, 7894, 1661, 7895, 1661,  // NOLINT
  7896, 1665, 7897, 1665, 7898, 1669, 7899, 1669, 7900, 1673, 7901, 1673, 7902, 1677, 7903, 1677,  // NOLINT
  7904, 1681, 7905, 1681, 7906, 1685, 7907, 1685, 7908, 1689, 7909, 1689, 7910, 1693, 7911, 1693,  // NOLINT
  7912, 1697, 7913, 1697, 7914, 1701, 7915, 1701, 7916, 1705, 7917, 1705, 7918, 1709, 7919, 1709,  // NOLINT
  7920, 1713, 7921, 1713, 7922, 1717, 7923, 1717, 7924, 1721, 7925, 1721, 7926, 1725, 7927, 1725,  // NOLINT
  7928, 1729, 7929, 1729, 1073749760, 1733, 7943, 1737, 1073749768, 1733, 7951, 1737, 1073749776, 1741, 7957, 1745,  // NOLINT
  1073749784, 1741, 7965, 1745, 1073749792, 1749, 7975, 1753, 1073749800, 1749, 7983, 1753, 1073749808, 1757, 7991, 1761,  // NOLINT
  1073749816, 1757, 7999, 1761, 1073749824, 1765, 8005, 1769, 1073749832, 1765, 8013, 1769, 8017, 1773, 8019, 1777,  // NOLINT
  8021, 1781, 8023, 1785, 8025, 1773, 8027, 1777, 8029, 1781, 8031, 1785, 1073749856, 1789, 8039, 1793,  // NOLINT
  1073749864, 1789, 8047, 1793, 1073749872, 1797, 8049, 1801, 1073749874, 1805, 8053, 1809, 1073749878, 1813, 8055, 1817,  // NOLINT
  1073749880, 1821, 8057, 1825, 1073749882, 1829, 8059, 1833, 1073749884, 1837, 8061, 1841, 1073749936, 1845, 8113, 1849,  // NOLINT
  1073749944, 1845, 8121, 1849, 1073749946, 1797, 8123, 1801, 8126, 697, 1073749960, 1805, 8139, 1809, 1073749968, 1853,  // NOLINT
  8145, 1857, 1073749976, 1853, 8153, 1857, 1073749978, 1813, 8155, 1817, 1073749984, 1861, 8161, 1865, 8165, 1869,  // NOLINT
  1073749992, 1861, 8169, 1865, 1073749994, 1829, 8171, 1833, 8172, 1869, 1073750008, 1821, 8185, 1825, 1073750010, 1837,  // NOLINT
  8187, 1841 };  // NOLINT
static const uint16_t kEcma262UnCanonicalizeMultiStrings0Size = 469;  // NOLINT
static const MultiCharacterSpecialCase<2> kEcma262UnCanonicalizeMultiStrings1[71] = {  // NOLINT
  {{8498, 8526}}, {{8544, 8560}}, {{8559, 8575}}, {{8579, 8580}},  // NOLINT
  {{9398, 9424}}, {{9423, 9449}}, {{11264, 11312}}, {{11310, 11358}},  // NOLINT
  {{11360, 11361}}, {{619, 11362}}, {{7549, 11363}}, {{637, 11364}},  // NOLINT
  {{570, 11365}}, {{574, 11366}}, {{11367, 11368}}, {{11369, 11370}},  // NOLINT
  {{11371, 11372}}, {{11381, 11382}}, {{11392, 11393}}, {{11394, 11395}},  // NOLINT
  {{11396, 11397}}, {{11398, 11399}}, {{11400, 11401}}, {{11402, 11403}},  // NOLINT
  {{11404, 11405}}, {{11406, 11407}}, {{11408, 11409}}, {{11410, 11411}},  // NOLINT
  {{11412, 11413}}, {{11414, 11415}}, {{11416, 11417}}, {{11418, 11419}},  // NOLINT
  {{11420, 11421}}, {{11422, 11423}}, {{11424, 11425}}, {{11426, 11427}},  // NOLINT
  {{11428, 11429}}, {{11430, 11431}}, {{11432, 11433}}, {{11434, 11435}},  // NOLINT
  {{11436, 11437}}, {{11438, 11439}}, {{11440, 11441}}, {{11442, 11443}},  // NOLINT
  {{11444, 11445}}, {{11446, 11447}}, {{11448, 11449}}, {{11450, 11451}},  // NOLINT
  {{11452, 11453}}, {{11454, 11455}}, {{11456, 11457}}, {{11458, 11459}},  // NOLINT
  {{11460, 11461}}, {{11462, 11463}}, {{11464, 11465}}, {{11466, 11467}},  // NOLINT
  {{11468, 11469}}, {{11470, 11471}}, {{11472, 11473}}, {{11474, 11475}},  // NOLINT
  {{11476, 11477}}, {{11478, 11479}}, {{11480, 11481}}, {{11482, 11483}},  // NOLINT
  {{11484, 11485}}, {{11486, 11487}}, {{11488, 11489}}, {{11490, 11491}},  // NOLINT
  {{4256, 11520}}, {{4293, 11557}}, {{kSentinel}} }; // NOLINT
static const uint16_t kEcma262UnCanonicalizeTable1Size = 133;  // NOLINT
static const int32_t kEcma262UnCanonicalizeTable1[266] = {
  306, 1, 334, 1, 1073742176, 5, 367, 9, 1073742192, 5, 383, 9, 387, 13, 388, 13,  // NOLINT
  1073743030, 17, 1231, 21, 1073743056, 17, 1257, 21, 1073744896, 25, 3118, 29, 1073744944, 25, 3166, 29,  // NOLINT
  3168, 33, 3169, 33, 3170, 37, 3171, 41, 3172, 45, 3173, 49, 3174, 53, 3175, 57,  // NOLINT
  3176, 57, 3177, 61, 3178, 61, 3179, 65, 3180, 65, 3189, 69, 3190, 69, 3200, 73,  // NOLINT
  3201, 73, 3202, 77, 3203, 77, 3204, 81, 3205, 81, 3206, 85, 3207, 85, 3208, 89,  // NOLINT
  3209, 89, 3210, 93, 3211, 93, 3212, 97, 3213, 97, 3214, 101, 3215, 101, 3216, 105,  // NOLINT
  3217, 105, 3218, 109, 3219, 109, 3220, 113, 3221, 113, 3222, 117, 3223, 117, 3224, 121,  // NOLINT
  3225, 121, 3226, 125, 3227, 125, 3228, 129, 3229, 129, 3230, 133, 3231, 133, 3232, 137,  // NOLINT
  3233, 137, 3234, 141, 3235, 141, 3236, 145, 3237, 145, 3238, 149, 3239, 149, 3240, 153,  // NOLINT
  3241, 153, 3242, 157, 3243, 157, 3244, 161, 3245, 161, 3246, 165, 3247, 165, 3248, 169,  // NOLINT
  3249, 169, 3250, 173, 3251, 173, 3252, 177, 3253, 177, 3254, 181, 3255, 181, 3256, 185,  // NOLINT
  3257, 185, 3258, 189, 3259, 189, 3260, 193, 3261, 193, 3262, 197, 3263, 197, 3264, 201,  // NOLINT
  3265, 201, 3266, 205, 3267, 205, 3268, 209, 3269, 209, 3270, 213, 3271, 213, 3272, 217,  // NOLINT
  3273, 217, 3274, 221, 3275, 221, 3276, 225, 3277, 225, 3278, 229, 3279, 229, 3280, 233,  // NOLINT
  3281, 233, 3282, 237, 3283, 237, 3284, 241, 3285, 241, 3286, 245, 3287, 245, 3288, 249,  // NOLINT
  3289, 249, 3290, 253, 3291, 253, 3292, 257, 3293, 257, 3294, 261, 3295, 261, 3296, 265,  // NOLINT
  3297, 265, 3298, 269, 3299, 269, 1073745152, 273, 3365, 277 };  // NOLINT
static const uint16_t kEcma262UnCanonicalizeMultiStrings1Size = 71;  // NOLINT
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


uchar UnicodeData::kMaxCodePoint = 65533;

int UnicodeData::GetByteCount() {
  return kUppercaseTable0Size * sizeof(int32_t)  // NOLINT
      + kUppercaseTable1Size * sizeof(int32_t)  // NOLINT
      + kUppercaseTable7Size * sizeof(int32_t)  // NOLINT
      + kLowercaseTable0Size * sizeof(int32_t)  // NOLINT
      + kLowercaseTable1Size * sizeof(int32_t)  // NOLINT
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
      + kToLowercaseMultiStrings7Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kToUppercaseMultiStrings0Size * sizeof(MultiCharacterSpecialCase<3>)  // NOLINT
      + kToUppercaseMultiStrings1Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kToUppercaseMultiStrings7Size * sizeof(MultiCharacterSpecialCase<3>)  // NOLINT
      + kEcma262CanonicalizeMultiStrings0Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kEcma262CanonicalizeMultiStrings1Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kEcma262CanonicalizeMultiStrings7Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kEcma262UnCanonicalizeMultiStrings0Size * sizeof(MultiCharacterSpecialCase<4>)  // NOLINT
      + kEcma262UnCanonicalizeMultiStrings1Size * sizeof(MultiCharacterSpecialCase<2>)  // NOLINT
      + kEcma262UnCanonicalizeMultiStrings7Size * sizeof(MultiCharacterSpecialCase<2>)  // NOLINT
      + kCanonicalizationRangeMultiStrings0Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kCanonicalizationRangeMultiStrings1Size * sizeof(MultiCharacterSpecialCase<1>)  // NOLINT
      + kCanonicalizationRangeMultiStrings7Size * sizeof(MultiCharacterSpecialCase<1>); // NOLINT
}

}  // namespace unicode
