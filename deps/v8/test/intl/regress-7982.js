// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure that maximize and minimize of locales work reasonbly.

assertEquals("zh-Hans-CN-u-ca-chinese", new Intl.Locale("zh-u-ca-Chinese").maximize().toString());
assertEquals("zh-u-ca-chinese", new Intl.Locale("zh-u-ca-Chinese").minimize().toString());
assertEquals("th-Thai-TH-u-nu-thai", new Intl.Locale("th-Thai-TH-u-nu-Thai").maximize().toString());
assertEquals("th-u-nu-thai", new Intl.Locale("th-Thai-TH-u-nu-Thai").minimize().toString());
assertEquals("th-Thai-TH-u-nu-thai", new Intl.Locale("th-u-nu-Thai").maximize().toString());
assertEquals("th-u-nu-thai", new Intl.Locale("th-u-nu-Thai").minimize().toString());
assertEquals("zh-Hans-CN-u-ca-chinese", new Intl.Locale("zh-CN-u-ca-chinese").maximize().toString());
assertEquals("zh-u-ca-chinese", new Intl.Locale("zh-CN-u-ca-chinese").minimize().toString());
assertEquals("zh-Hant-TW-u-ca-chinese", new Intl.Locale("zh-TW-u-ca-chinese").maximize().toString());
assertEquals("zh-TW-u-ca-chinese", new Intl.Locale("zh-TW-u-ca-chinese").minimize().toString());
assertEquals("zh-Hant-HK-u-ca-chinese", new Intl.Locale("zh-HK-u-ca-chinese").maximize().toString());
assertEquals("zh-HK-u-ca-chinese", new Intl.Locale("zh-HK-u-ca-chinese").minimize().toString());
assertEquals("zh-Hant-TW-u-ca-chinese", new Intl.Locale("zh-Hant-u-ca-chinese").maximize().toString());
assertEquals("zh-Hant-u-ca-chinese", new Intl.Locale("zh-Hant-u-ca-chinese").minimize().toString());
assertEquals("zh-Hans-CN-u-ca-chinese", new Intl.Locale("zh-Hans-u-ca-chinese").maximize().toString());
assertEquals("zh-u-ca-chinese", new Intl.Locale("zh-Hans-u-ca-chinese").minimize().toString());
assertEquals("zh-Hant-CN-u-ca-chinese", new Intl.Locale("zh-Hant-CN-u-ca-chinese").maximize().toString());
assertEquals("zh-Hant-CN-u-ca-chinese", new Intl.Locale("zh-Hant-CN-u-ca-chinese").minimize().toString());
assertEquals("zh-Hans-CN-u-ca-chinese", new Intl.Locale("zh-Hans-CN-u-ca-chinese").maximize().toString());
assertEquals("zh-u-ca-chinese", new Intl.Locale("zh-Hans-CN-u-ca-chinese").minimize().toString());
assertEquals("zh-Hant-TW-u-ca-chinese", new Intl.Locale("zh-Hant-TW-u-ca-chinese").maximize().toString());
assertEquals("zh-TW-u-ca-chinese", new Intl.Locale("zh-Hant-TW-u-ca-chinese").minimize().toString());
assertEquals("zh-Hans-TW-u-ca-chinese", new Intl.Locale("zh-Hans-TW-u-ca-chinese").maximize().toString());
assertEquals("zh-Hans-TW-u-ca-chinese", new Intl.Locale("zh-Hans-TW-u-ca-chinese").minimize().toString());
assertEquals("zh-Hant-HK-u-ca-chinese", new Intl.Locale("zh-Hant-HK-u-ca-chinese").maximize().toString());
assertEquals("zh-HK-u-ca-chinese", new Intl.Locale("zh-Hant-HK-u-ca-chinese").minimize().toString());
assertEquals("zh-Hans-HK-u-ca-chinese", new Intl.Locale("zh-Hans-HK-u-ca-chinese").maximize().toString());
assertEquals("zh-Hans-HK-u-ca-chinese", new Intl.Locale("zh-Hans-HK-u-ca-chinese").minimize().toString());
