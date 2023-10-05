// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-intl-locale-info-func
// Check direction of minority right-to-left languages

assertEquals('rtl', (new Intl.Locale("ar")).getTextInfo().direction, "ar");
assertEquals('rtl', (new Intl.Locale("fa")).getTextInfo().direction, "fa");
assertEquals('rtl', (new Intl.Locale("ae")).getTextInfo().direction, "ae");
assertEquals('rtl', (new Intl.Locale("arc")).getTextInfo().direction, "arc");
assertEquals('rtl', (new Intl.Locale("bcc")).getTextInfo().direction, "bcc");
assertEquals('rtl', (new Intl.Locale("bqi")).getTextInfo().direction, "bqi");
assertEquals('rtl', (new Intl.Locale("dv")).getTextInfo().direction, "dv");
assertEquals('rtl', (new Intl.Locale("glk")).getTextInfo().direction, "glk");
assertEquals('rtl', (new Intl.Locale("mzn")).getTextInfo().direction, "mzn");
assertEquals('rtl', (new Intl.Locale("nqo")).getTextInfo().direction, "nqo");
assertEquals('rtl', (new Intl.Locale("pnb")).getTextInfo().direction, "pnb");
assertEquals('rtl', (new Intl.Locale("sd")).getTextInfo().direction, "sd");
assertEquals('rtl', (new Intl.Locale("ug")).getTextInfo().direction, "ug");
assertEquals('rtl', (new Intl.Locale("yi")).getTextInfo().direction, "yi");
assertEquals('ltr', (new Intl.Locale("ku")).getTextInfo().direction, "ku");
assertEquals('rtl', (new Intl.Locale("ku-Arab")).getTextInfo().direction, "ku-Arab");
