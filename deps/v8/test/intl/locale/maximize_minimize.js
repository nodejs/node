// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-locale

// Make sure that maximize and minimize of all locales work reasonbly.

assertEquals(new Intl.Locale("zh-TW").maximize().toString(), "zh-Hant-TW",
    "zh-TW should maximize to zh-Hant-TW");
assertEquals(new Intl.Locale("zh-Hant-TW").minimize().toString(), "zh-TW",
    "zh-Hant-TW should minimize to zh-TW");
assertEquals(new Intl.Locale("zh-Hans-CN").minimize().toString(), "zh",
    "zh-Hans-CN should minimize to zh");
assertEquals(new Intl.Locale("zh-CN").minimize().toString(), "zh",
    "zh-CN should minimize to zh");
assertEquals(new Intl.Locale("zh-Hans").minimize().toString(), "zh",
    "zh-Hans should minimize to zh");

function assertExpandRoundTrip(loc) {
  assertEquals(
      loc.toString(), loc.maximize().minimize().toString(), loc.toString());
  assertEquals(
      loc.toString(), loc.minimize().toString(), loc.toString());
  assertTrue(
      loc.maximize().toString().length > loc.toString().length, loc.toString());
}

var simpleLocales = [
  "af", "agq", "ak", "am", "ar", "asa", "ast", "as", "az", "bas", "bem", "be",
  "bez", "bg", "bm", "bn", "bo", "br", "brx", "bs", "ca", "ccp", "ce", "cgg",
  "chr", "ckb", "cs", "cu", "cy", "dav", "da", "de", "dje", "dsb", "dua", "dyo",
  "dz", "ebu", "ee", "el", "en", "eo", "es", "et", "eu", "ewo", "fa", "ff",
  "fil", "fi", "fo", "fr", "fur", "fy", "ga", "gd", "gl", "gsw", "gu", "guz",
  "gv", "haw", "ha", "he", "hi", "hr", "hsb", "hu", "hy", "id", "ig", "ii",
  "is", "it", "ja", "jgo", "jmc", "kab", "kam", "ka", "kde", "kea", "khq", "ki",
  "kkj", "kk", "kln", "kl", "km", "kn", "kok", "ko", "ksb", "ksf", "ksh", "ks",
  "kw", "ky", "lag", "lb", "lg", "lkt", "ln", "lo", "lrc", "lt", "luo", "lu",
  "luy", "lv", "mas", "mer", "mfe", "mgh", "mgo", "mg", "mk", "ml", "mn", "mr",
  "ms", "mt", "mua", "my", "mzn", "naq", "nb", "nds", "nd", "ne", "nl", "nmg",
  "nnh", "nn", "nus", "nyn", "om", "or", "os", "pa", "pl", "prg", "ps", "pt",
  "qu", "rm", "rn", "rof", "ro", "ru", "rwk", "rw", "sah", "saq", "sbp", "sd",
  "seh", "ses", "se", "sg", "shi", "si", "sk", "sl", "smn", "sn", "so", "sq",
  "sr", "sv", "sw", "ta", "teo", "te", "tg", "th", "ti", "tk", "to", "tr", "tt",
  "twq", "tzm", "ug", "uk", "ur", "uz", "vai", "vi", "vo", "vun", "wae", "wo",
  "xog", "yav", "yi", "yo", "yue", "zgh", "zh", "zu"];
for (var i = 0; i < simpleLocales.length; i++) {
  assertExpandRoundTrip(new Intl.Locale(simpleLocales[i]));
}

function assertReduceRoundTrip(loc) {
  assertEquals(
      loc.minimize().toString(), loc.maximize().minimize().toString(),
      loc.toString());
  assertEquals(
      loc.maximize().toString(), loc.minimize().maximize().toString(),
      loc.toString());
  assertTrue(
      loc.maximize().toString().length >= loc.toString().length, loc.toString());
  assertTrue(
      loc.minimize().toString().length <= loc.toString().length, loc.toString());
}

var complexLocales = [
  "af-NA", "af-ZA", "agq-CM", "ak-GH", "am-ET", "ar-001", "ar-AE", "ar-BH",
  "ar-DJ", "ar-DZ", "ar-EG", "ar-EH", "ar-ER", "ar-IL", "ar-IQ", "ar-JO",
  "ar-KM", "ar-KW", "ar-LB", "ar-LY", "ar-MA", "ar-MR", "ar-OM", "ar-PS",
  "ar-QA", "ar-SA", "ar-SD", "ar-SO", "ar-SS", "ar-SY", "ar-TD", "ar-TN",
  "ar-YE", "asa-TZ", "as-IN", "ast-ES", "az-Cyrl-AZ", "az-Cyrl", "az-Latn-AZ",
  "az-Latn", "bas-CM", "be-BY", "bem-ZM", "bez-TZ", "bg-BG", "bm-ML", "bn-BD",
  "bn-IN", "bo-CN", "bo-IN", "br-FR", "brx-IN", "bs-Cyrl-BA", "bs-Cyrl",
  "bs-Latn-BA", "bs-Latn", "ca-AD", "ca-ES", "ca-FR", "ca-IT",
  "ccp-BD", "ccp-IN", "ce-RU", "cgg-UG", "chr-US", "ckb-Arab-IQ", "ckb-Arab-IR",
  "ckb-Arab", "ckb-IQ", "ckb-IR", "ckb-Latn-IQ", "ckb-Latn", "cs-CZ", "cu-RU",
  "cy-GB", "da-DK", "da-GL", "dav-KE", "de-AT", "de-BE", "de-CH", "de-DE",
  "de-IT", "de-LI", "de-LU", "dje-NE", "dsb-DE", "dua-CM", "dyo-SN", "dz-BT",
  "ebu-KE", "ee-GH", "ee-TG", "el-CY", "el-GR", "en-001", "en-150", "en-AG",
  "en-AI", "en-AS", "en-AT", "en-AU", "en-BB", "en-BE", "en-BI", "en-BM",
  "en-BS", "en-BW", "en-BZ", "en-CA", "en-CC", "en-CH", "en-CK", "en-CM",
  "en-CX", "en-CY", "en-DE", "en-DG", "en-DK", "en-DM", "en-ER", "en-FI",
  "en-FJ", "en-FK", "en-FM", "en-GB", "en-GD", "en-GG", "en-GH", "en-GI",
  "en-GM", "en-GU", "en-GY", "en-HK", "en-IE", "en-IL", "en-IM", "en-IN",
  "en-IO", "en-JE", "en-JM", "en-KE", "en-KI", "en-KN", "en-KY", "en-LC",
  "en-LR", "en-LS", "en-MG", "en-MH", "en-MO", "en-MP", "en-MS", "en-MT",
  "en-MU", "en-MW", "en-MY", "en-NA", "en-NF", "en-NG", "en-NL", "en-NR",
  "en-NU", "en-NZ", "en-PG", "en-PH", "en-PK", "en-PN", "en-PR", "en-PW",
  "en-RW", "en-SB", "en-SC", "en-SD", "en-SE", "en-SG", "en-SH", "en-SI",
  "en-SL", "en-SS", "en-SX", "en-SZ", "en-TC", "en-TK", "en-TO", "en-TT",
  "en-TV", "en-TZ", "en-UG", "en-UM", "en-US", "en-VC",
  "en-VG", "en-VI", "en-VU", "en-WS", "en-ZA", "en-ZM", "en-ZW", "eo-001",
  "es-419", "es-AR", "es-BO", "es-BR", "es-BZ", "es-CL", "es-CO", "es-CR",
  "es-CU", "es-DO", "es-EA", "es-EC", "es-ES", "es-GQ", "es-GT", "es-HN",
  "es-IC", "es-MX", "es-NI", "es-PA", "es-PE", "es-PH", "es-PR", "es-PY",
  "es-SV", "es-US", "es-UY", "es-VE", "et-EE", "eu-ES", "ewo-CM", "fa-AF",
  "fa-IR", "ff-CM", "ff-GN", "ff-MR", "ff-SN", "fi-FI", "fil-PH", "fo-DK",
  "fo-FO", "fr-BE", "fr-BF", "fr-BI", "fr-BJ", "fr-BL", "fr-CA", "fr-CD",
  "fr-CF", "fr-CG", "fr-CH", "fr-CI", "fr-CM", "fr-DJ", "fr-DZ", "fr-FR",
  "fr-GA", "fr-GF", "fr-GN", "fr-GP", "fr-GQ", "fr-HT", "fr-KM", "fr-LU",
  "fr-MA", "fr-MC", "fr-MF", "fr-MG", "fr-ML", "fr-MQ", "fr-MR", "fr-MU",
  "fr-NC", "fr-NE", "fr-PF", "fr-PM", "fr-RE", "fr-RW", "fr-SC", "fr-SN",
  "fr-SY", "fr-TD", "fr-TG", "fr-TN", "fr-VU", "fr-WF", "fr-YT", "fur-IT",
  "fy-NL", "ga-IE", "gd-GB", "gl-ES", "gsw-CH", "gsw-FR", "gsw-LI", "gu-IN",
  "guz-KE", "gv-IM", "ha-GH", "ha-NE", "ha-NG", "haw-US", "he-IL", "hi-IN",
  "hr-BA", "hr-HR", "hsb-DE", "hu-HU", "hy-AM", "id-ID", "ig-NG", "ii-CN",
  "is-IS", "it-CH", "it-IT", "it-SM", "it-VA", "ja-JP", "jgo-CM", "jmc-TZ",
  "kab-DZ", "ka-GE", "kam-KE", "kde-TZ", "kea-CV", "khq-ML", "ki-KE",
  "kkj-CM", "kk-KZ", "kl-GL", "kln-KE", "km-KH", "kn-IN", "kok-IN", "ko-KP",
  "ko-KR", "ksb-TZ", "ksf-CM", "ksh-DE", "ks-IN", "kw-GB", "ky-KG", "lag-TZ",
  "lb-LU", "lg-UG", "lkt-US", "ln-AO", "ln-CD", "ln-CF", "ln-CG", "lo-LA",
  "lrc-IQ", "lrc-IR", "lt-LT", "lu-CD", "luo-KE", "luy-KE", "lv-LV", "mas-KE",
  "mas-TZ", "mer-KE", "mfe-MU", "mgh-MZ", "mg-MG", "mgo-CM", "mk-MK", "ml-IN",
  "mn-MN", "mr-IN", "ms-BN", "ms-MY", "ms-SG", "mt-MT", "mua-CM", "my-MM",
  "mzn-IR", "naq-NA", "nb-NO", "nb-SJ", "nds-DE", "nds-NL", "nd-ZW", "ne-IN",
  "ne-NP", "nl-AW", "nl-BE", "nl-BQ", "nl-CW", "nl-NL", "nl-SR", "nl-SX",
  "nmg-CM", "nnh-CM", "nn-NO", "nus-SS", "nyn-UG", "om-ET", "om-KE",
  "or-IN", "os-GE", "os-RU", "pa-Arab-PK", "pa-Guru-IN", "pa-Guru",
  "pl-PL", "prg-001", "ps-AF", "pt-AO", "pt-BR", "pt-CH", "pt-CV", "pt-GQ",
  "pt-GW", "pt-LU", "pt-MO", "pt-MZ", "pt-PT", "pt-ST", "pt-TL", "qu-BO",
  "qu-EC", "qu-PE", "rm-CH", "rn-BI", "rof-TZ", "ro-MD", "ro-RO", "ru-BY",
  "ru-KG", "ru-KZ", "ru-MD", "ru-RU", "ru-UA", "rwk-TZ", "rw-RW", "sah-RU",
  "saq-KE", "sbp-TZ", "sd-PK", "se-FI", "seh-MZ", "se-NO", "se-SE", "ses-ML",
  "sg-CF", "shi-Latn-MA", "shi-Latn", "shi-Tfng-MA", "shi-Tfng", "si-LK",
  "sk-SK", "sl-SI", "smn-FI", "sn-ZW", "so-DJ", "so-ET", "so-KE", "so-SO",
  "sq-AL", "sq-MK", "sq-XK", "sr-Cyrl-BA", "sr-Cyrl-ME", "sr-Cyrl-RS",
  "sr-Cyrl-XK", "sr-Cyrl", "sr-Latn-BA", "sr-Latn-ME", "sr-Latn-RS",
  "sr-Latn-XK", "sr-Latn", "sv-AX", "sv-FI", "sv-SE", "sw-CD", "sw-KE",
  "sw-TZ", "sw-UG", "ta-IN", "ta-LK", "ta-MY", "ta-SG", "te-IN", "teo-KE",
  "teo-UG", "tg-TJ", "th-TH", "ti-ER", "ti-ET", "tk-TM", "to-TO", "tr-CY",
  "tr-TR", "tt-RU", "twq-NE", "tzm-MA", "ug-CN", "uk-UA", "ur-IN", "ur-PK",
  "uz-Arab-AF", "uz-Cyrl-UZ", "uz-Cyrl", "uz-Latn-UZ", "uz-Latn",
  "vai-Latn-LR", "vai-Latn", "vai-Vaii-LR", "vai-Vaii", "vi-VN", "vo-001",
  "vun-TZ", "wae-CH", "wo-SN", "xog-UG", "yav-CM", "yi-001", "yo-BJ", "yo-NG",
  "yue-Hans-CN", "yue-Hant-HK", "yue-Hant", "zgh-MA", "zh-Hans-CN",
  "zh-Hans-HK", "zh-Hans-MO", "zh-Hans-SG", "zh-Hans", "zh-Hant-HK",
  "zh-Hant-MO", "zh-Hant-TW", "zu-ZA"];
for (var i = 0; i < complexLocales.length; i++) {
  assertReduceRoundTrip(new Intl.Locale(complexLocales[i]));
}
