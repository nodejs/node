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
#ifndef LIEF_PE_RESOURCE_LANG_H
#define LIEF_PE_RESOURCE_LANG_H
#include <cstdint>
namespace LIEF {
namespace PE {

enum class RESOURCE_LANGS  {
  NEUTRAL        = 0x00,
  INVARIANT      = 0x7f,
  AFRIKAANS      = 0x36,
  ALBANIAN       = 0x1c,
  ARABIC         = 0x01,
  ARMENIAN       = 0x2b,
  ASSAMESE       = 0x4d,
  AZERI          = 0x2c,
  BASQUE         = 0x2d,
  BELARUSIAN     = 0x23,
  BANGLA         = 0x45,
  BULGARIAN      = 0x02,
  CATALAN        = 0x03,
  CHINESE        = 0x04,
  CROATIAN       = 0x1a,
  BOSNIAN        = 0x1a,
  CZECH          = 0x05,
  DANISH         = 0x06,
  DIVEHI         = 0x65,
  DUTCH          = 0x13,
  ENGLISH        = 0x09,
  ESTONIAN       = 0x25,
  FAEROESE       = 0x38,
  FARSI          = 0x29,
  FINNISH        = 0x0b,
  FRENCH         = 0x0c,
  GALICIAN       = 0x56,
  GEORGIAN       = 0x37,
  GERMAN         = 0x07,
  GREEK          = 0x08,
  GUJARATI       = 0x47,
  HEBREW         = 0x0d,
  HINDI          = 0x39,
  HUNGARIAN      = 0x0e,
  ICELANDIC      = 0x0f,
  INDONESIAN     = 0x21,
  ITALIAN        = 0x10,
  JAPANESE       = 0x11,
  KANNADA        = 0x4b,
  KASHMIRI       = 0x60,
  KAZAK          = 0x3f,
  KONKANI        = 0x57,
  KOREAN         = 0x12,
  KYRGYZ         = 0x40,
  LATVIAN        = 0x26,
  LITHUANIAN     = 0x27,
  MACEDONIAN     = 0x2f,
  MALAY          = 0x3e,
  MALAYALAM      = 0x4c,
  MANIPURI       = 0x58,
  MARATHI        = 0x4e,
  MONGOLIAN      = 0x50,
  NEPALI         = 0x61,
  NORWEGIAN      = 0x14,
  ORIYA          = 0x48,
  POLISH         = 0x15,
  PORTUGUESE     = 0x16,
  PUNJABI        = 0x46,
  ROMANIAN       = 0x18,
  RUSSIAN        = 0x19,
  SANSKRIT       = 0x4f,
  SERBIAN        = 0x1a,
  SINDHI         = 0x59,
  SLOVAK         = 0x1b,
  SLOVENIAN      = 0x24,
  SPANISH        = 0x0a,
  SWAHILI        = 0x41,
  SWEDISH        = 0x1d,
  SYRIAC         = 0x5a,
  TAMIL          = 0x49,
  TATAR          = 0x44,
  TELUGU         = 0x4a,
  THAI           = 0x1e,
  TURKISH        = 0x1f,
  UKRAINIAN      = 0x22,
  URDU           = 0x20,
  UZBEK          = 0x43,
  VIETNAMESE     = 0x2a,
  GAELIC         = 0x3c,
  MALTESE        = 0x3a,
  MAORI          = 0x28,
  RHAETO_ROMANCE = 0x17,
  SAMI           = 0x3b,
  SORBIAN        = 0x2e,
  SUTU           = 0x30,
  TSONGA         = 0x31,
  TSWANA         = 0x32,
  VENDA          = 0x33,
  XHOSA          = 0x34,
  ZULU           = 0x35,
  ESPERANTO      = 0x8f,
  WALON          = 0x90,
  CORNISH        = 0x91,
  WELSH          = 0x92,
  BRETON         = 0x93,
  INUKTITUT      = 0x5d,
  IRISH          = 0x3C,
  LOWER_SORBIAN  = 0x2E,
  PULAR          = 0x67,
  QUECHUA        = 0x6B,
  TAMAZIGHT      = 0x5F,
  TIGRINYA       = 0x73,
  VALENCIAN      = 0x03,
};

static constexpr uint32_t lang_from_id(uint32_t id) {
  return id & 0x3ff;
}

static constexpr uint32_t sublang_from_id(uint32_t id) {
  return id >> 10;
}

static constexpr uint32_t encode_lang(uint32_t lang, uint32_t sublang) {
  return (sublang << 10) | lang;
}


}
}
#endif
