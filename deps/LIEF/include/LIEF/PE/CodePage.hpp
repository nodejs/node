/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PE_CODE_PAGE
#define LIEF_PE_CODE_PAGE
#include <cstdint>
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

/// Code page from https://docs.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
enum class CODE_PAGES : uint32_t  {
  IBM037                  = 37,    /**< IBM EBCDIC US-Canada */
  IBM437                  = 437,   /**< OEM United States */
  IBM500                  = 500,   /**< IBM EBCDIC International */
  ASMO_708                = 708,   /**< Arabic (ASMO 708) */
  DOS_720                 = 720,   /**< Arabic (Transparent ASMO); Arabic (DOS) */
  IBM737                  = 737,   /**< OEM Greek (formerly 437G); Greek (DOS) */
  IBM775                  = 775,   /**< OEM Baltic; Baltic (DOS) */
  IBM850                  = 850,   /**< OEM Multilingual Latin 1; Western European (DOS) */
  IBM852                  = 852,   /**< OEM Latin 2; Central European (DOS) */
  IBM855                  = 855,   /**< OEM Cyrillic (primarily Russian) */
  IBM857                  = 857,   /**< OEM Turkish; Turkish (DOS) */
  IBM00858                = 858,   /**< OEM Multilingual Latin 1 + Euro symbol */
  IBM860                  = 860,   /**< OEM Portuguese; Portuguese (DOS) */
  IBM861                  = 861,   /**< OEM Icelandic; Icelandic (DOS) */
  DOS_862                 = 862,   /**< OEM Hebrew; Hebrew (DOS) */
  IBM863                  = 863,   /**< OEM French Canadian; French Canadian (DOS) */
  IBM864                  = 864,   /**< OEM Arabic; Arabic (864) */
  IBM865                  = 865,   /**< OEM Nordic; Nordic (DOS) */
  CP866                   = 866,   /**< OEM Russian; Cyrillic (DOS) */
  IBM869                  = 869,   /**< OEM Modern Greek; Greek, Modern (DOS) */
  IBM870                  = 870,   /**< IBM EBCDIC Multilingual/ROECE (Latin 2); IBM EBCDIC Multilingual Latin 2 */
  WINDOWS_874             = 874,   /**< ANSI/OEM Thai (same as 28605, ISO 8859-15); Thai (Windows) */
  CP875                   = 875,   /**< IBM EBCDIC Greek Modern */
  SHIFT_JIS               = 932,   /**< ANSI/OEM Japanese; Japanese (Shift-JIS) */
  GB2312                  = 936,   /**< ANSI/OEM Simplified Chinese (PRC, Singapore); Chinese Simplified (GB2312) */
  KS_C_5601_1987          = 949,   /**< ANSI/OEM Korean (Unified Hangul Code) */
  BIG5                    = 950,   /**< ANSI/OEM Traditional Chinese (Taiwan; Hong Kong SAR, PRC); Chinese Traditional (Big5) */
  IBM1026                 = 1026,  /**< IBM EBCDIC Turkish (Latin 5) */
  IBM01047                = 1047,  /**< IBM EBCDIC Latin 1/Open System */
  IBM01140                = 1140,  /**< IBM EBCDIC US-Canada (037 + Euro symbol); IBM EBCDIC (US-Canada-Euro) */
  IBM01141                = 1141,  /**< IBM EBCDIC Germany (20273 + Euro symbol); IBM EBCDIC (Germany-Euro) */
  IBM01142                = 1142,  /**< IBM EBCDIC Denmark-Norway (20277 + Euro symbol); IBM EBCDIC (Denmark-Norway-Euro) */
  IBM01143                = 1143,  /**< IBM EBCDIC Finland-Sweden (20278 + Euro symbol); IBM EBCDIC (Finland-Sweden-Euro) */
  IBM01144                = 1144,  /**< IBM EBCDIC Italy (20280 + Euro symbol); IBM EBCDIC (Italy-Euro) */
  IBM01145                = 1145,  /**< IBM EBCDIC Latin America-Spain (20284 + Euro symbol); IBM EBCDIC (Spain-Euro) */
  IBM01146                = 1146,  /**< IBM EBCDIC United Kingdom (20285 + Euro symbol); IBM EBCDIC (UK-Euro) */
  IBM01147                = 1147,  /**< IBM EBCDIC France (20297 + Euro symbol); IBM EBCDIC (France-Euro) */
  IBM01148                = 1148,  /**< IBM EBCDIC International (500 + Euro symbol); IBM EBCDIC (International-Euro) */
  IBM01149                = 1149,  /**< IBM EBCDIC Icelandic (20871 + Euro symbol); IBM EBCDIC (Icelandic-Euro) */
  UTF_16                  = 1200,  /**< Unicode UTF-16, little endian byte order (BMP of ISO 10646); available only to managed applications */
  UNICODEFFFE             = 1201,  /**< Unicode UTF-16, big endian byte order; available only to managed applications */
  WINDOWS_1250            = 1250,  /**< ANSI Central European; Central European (Windows) */
  WINDOWS_1251            = 1251,  /**< ANSI Cyrillic; Cyrillic (Windows) */
  WINDOWS_1252            = 1252,  /**< ANSI Latin 1; Western European (Windows) */
  WINDOWS_1253            = 1253,  /**< ANSI Greek; Greek (Windows) */
  WINDOWS_1254            = 1254,  /**< ANSI Turkish; Turkish (Windows) */
  WINDOWS_1255            = 1255,  /**< ANSI Hebrew; Hebrew (Windows) */
  WINDOWS_1256            = 1256,  /**< ANSI Arabic; Arabic (Windows) */
  WINDOWS_1257            = 1257,  /**< ANSI Baltic; Baltic (Windows) */
  WINDOWS_1258            = 1258,  /**< ANSI/OEM Vietnamese; Vietnamese (Windows) */
  JOHAB                   = 1361,  /**< Korean (Johab) */
  MACINTOSH               = 10000, /**< MAC Roman; Western European (Mac) */
  X_MAC_JAPANESE          = 10001, /**< Japanese (Mac) */
  X_MAC_CHINESETRAD       = 10002, /**< MAC Traditional Chinese (Big5); Chinese Traditional (Mac) */
  X_MAC_KOREAN            = 10003, /**< Korean (Mac) */
  X_MAC_ARABIC            = 10004, /**< Arabic (Mac) */
  X_MAC_HEBREW            = 10005, /**< Hebrew (Mac) */
  X_MAC_GREEK             = 10006, /**< Greek (Mac) */
  X_MAC_CYRILLIC          = 10007, /**< Cyrillic (Mac) */
  X_MAC_CHINESESIMP       = 10008, /**< MAC Simplified Chinese (GB 2312); Chinese Simplified (Mac) */
  X_MAC_ROMANIAN          = 10010, /**< Romanian (Mac) */
  X_MAC_UKRAINIAN         = 10017, /**< Ukrainian (Mac) */
  X_MAC_THAI              = 10021, /**< Thai (Mac) */
  X_MAC_CE                = 10029, /**< MAC Latin 2; Central European (Mac) */
  X_MAC_ICELANDIC         = 10079, /**< Icelandic (Mac) */
  X_MAC_TURKISH           = 10081, /**< Turkish (Mac) */
  X_MAC_CROATIAN          = 10082, /**< Croatian (Mac) */
  UTF_32                  = 12000, /**< Unicode UTF-32, little endian byte order; available only to managed applications */
  UTF_32BE                = 12001, /**< Unicode UTF-32, big endian byte order; available only to managed applications */
  X_CHINESE_CNS           = 20000, /**< CNS Taiwan; Chinese Traditional (CNS) */
  X_CP20001               = 20001, /**< TCA Taiwan */
  X_CHINESE_ETEN          = 20002, /**< Eten Taiwan; Chinese Traditional (Eten) */
  X_CP20003               = 20003, /**< IBM5550 Taiwan */
  X_CP20004               = 20004, /**< TeleText Taiwan */
  X_CP20005               = 20005, /**< Wang Taiwan */
  X_IA5                   = 20105, /**< IA5 (IRV International Alphabet No. 5, 7-bit); Western European (IA5) */
  X_IA5_GERMAN            = 20106, /**< IA5 German (7-bit) */
  X_IA5_SWEDISH           = 20107, /**< IA5 Swedish (7-bit) */
  X_IA5_NORWEGIAN         = 20108, /**< IA5 Norwegian (7-bit) */
  US_ASCII                = 20127, /**< US-ASCII (7-bit) */
  X_CP20261               = 20261, /**< T.61 */
  X_CP20269               = 20269, /**< ISO 6937 Non-Spacing Accent */
  IBM273                  = 20273, /**< IBM EBCDIC Germany */
  IBM277                  = 20277, /**< IBM EBCDIC Denmark-Norway */
  IBM278                  = 20278, /**< IBM EBCDIC Finland-Sweden */
  IBM280                  = 20280, /**< IBM EBCDIC Italy */
  IBM284                  = 20284, /**< IBM EBCDIC Latin America-Spain */
  IBM285                  = 20285, /**< IBM EBCDIC United Kingdom */
  IBM290                  = 20290, /**< IBM EBCDIC Japanese Katakana Extended */
  IBM297                  = 20297, /**< IBM EBCDIC France */
  IBM420                  = 20420, /**< IBM EBCDIC Arabic */
  IBM423                  = 20423, /**< IBM EBCDIC Greek */
  IBM424                  = 20424, /**< IBM EBCDIC Hebrew */
  X_EBCDIC_KOREANEXTENDED = 20833, /**< IBM EBCDIC Korean Extended */
  IBM_THAI                = 20838, /**< IBM EBCDIC Thai */
  KOI8_R                  = 20866, /**< Russian (KOI8-R); Cyrillic (KOI8-R) */
  IBM871                  = 20871, /**< IBM EBCDIC Icelandic */
  IBM880                  = 20880, /**< IBM EBCDIC Cyrillic Russian */
  IBM905                  = 20905, /**< IBM EBCDIC Turkish */
  IBM00924                = 20924, /**< IBM EBCDIC Latin 1/Open System (1047 + Euro symbol) */
  EUC_JP_JIS              = 20932, /**< Japanese (JIS 0208-1990 and 0121-1990) */
  X_CP20936               = 20936, /**< Simplified Chinese (GB2312); Chinese Simplified (GB2312-80) */
  X_CP20949               = 20949, /**< Korean Wansung */
  CP1025                  = 21025, /**< IBM EBCDIC Cyrillic Serbian-Bulgarian */
  KOI8_U                  = 21866, /**< Ukrainian (KOI8-U); Cyrillic (KOI8-U) */
  ISO_8859_1              = 28591, /**< ISO 8859-1 Latin 1; Western European (ISO) */
  ISO_8859_2              = 28592, /**< ISO 8859-2 Central European; Central European (ISO) */
  ISO_8859_3              = 28593, /**< ISO 8859-3 Latin 3 */
  ISO_8859_4              = 28594, /**< ISO 8859-4 Baltic */
  ISO_8859_5              = 28595, /**< ISO 8859-5 Cyrillic */
  ISO_8859_6              = 28596, /**< ISO 8859-6 Arabic */
  ISO_8859_7              = 28597, /**< ISO 8859-7 Greek */
  ISO_8859_8              = 28598, /**< ISO 8859-8 Hebrew; Hebrew (ISO-Visual) */
  ISO_8859_9              = 28599, /**< ISO 8859-9 Turkish */
  ISO_8859_13             = 28603, /**< ISO 8859-13 Estonian */
  ISO_8859_15             = 28605, /**< ISO 8859-15 Latin 9 */
  X_EUROPA                = 29001, /**< Europa 3 */
  ISO_8859_8_I            = 38598, /**< ISO 8859-8 Hebrew; Hebrew (ISO-Logical) */
  ISO_2022_JP             = 50220, /**< ISO 2022 Japanese with no halfwidth Katakana; Japanese (JIS) */
  CSISO2022JP             = 50221, /**< ISO 2022 Japanese with halfwidth Katakana; Japanese (JIS-Allow 1 byte Kana) */
  ISO_2022_JP_JIS         = 50222, /**< ISO 2022 Japanese JIS X 0201-1989; Japanese (JIS-Allow 1 byte Kana - SO/SI) */
  ISO_2022_KR             = 50225, /**< ISO 2022 Korean */
  X_CP50227               = 50227, /**< ISO 2022 Simplified Chinese; Chinese Simplified (ISO 2022) */
  EUC_JP                  = 51932, /**< EUC Japanese */
  EUC_CN                  = 51936, /**< EUC Simplified Chinese; Chinese Simplified (EUC) */
  EUC_KR                  = 51949, /**< EUC Korean */
  HZ_GB_2312              = 52936, /**< HZ-GB2312 Simplified Chinese; Chinese Simplified (HZ) */
  GB18030                 = 54936, /**< Windows XP and later: GB18030 Simplified Chinese (4 byte); Chinese Simplified (GB18030) */
  X_ISCII_DE              = 57002, /**< ISCII Devanagari */
  X_ISCII_BE              = 57003, /**< ISCII Bengali */
  X_ISCII_TA              = 57004, /**< ISCII Tamil */
  X_ISCII_TE              = 57005, /**< ISCII Telugu */
  X_ISCII_AS              = 57006, /**< ISCII Assamese */
  X_ISCII_OR              = 57007, /**< ISCII Oriya */
  X_ISCII_KA              = 57008, /**< ISCII Kannada */
  X_ISCII_MA              = 57009, /**< ISCII Malayalam */
  X_ISCII_GU              = 57010, /**< ISCII Gujarati */
  X_ISCII_PA              = 57011, /**< ISCII Punjabi */
  UTF_7                   = 65000, /**< Unicode (UTF-7) */
  UTF_8                   = 65001, /**< Unicode (UTF-8) */
};

LIEF_API const char* to_string(CODE_PAGES e);

}
}
#endif
