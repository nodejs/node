// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// test the numberingSystem is set correctly via -u-nu-
let dtf = new Intl.DateTimeFormat(["en-u-ba-rfoo-nu-arab-fo-obar"]);
assertEquals("arab", dtf.resolvedOptions().numberingSystem);
assertEquals("en-u-nu-arab", dtf.resolvedOptions().locale);

let nf = new Intl.NumberFormat(["en-u-ba-rfoo-nu-arab-fo-obar"]);
assertEquals("arab", nf.resolvedOptions().numberingSystem);
assertEquals("١٢٣", nf.format(123));
assertEquals("en-u-nu-arab", nf.resolvedOptions().locale);

dtf = new Intl.DateTimeFormat(["en-u-ba-rfoo-nu-thai-fo-obar"]);
assertEquals("thai", dtf.resolvedOptions().numberingSystem);
assertEquals("en-u-nu-thai", dtf.resolvedOptions().locale);

nf = new Intl.NumberFormat(["en-u-ba-rfoo-nu-thai-fo-obar"]);
assertEquals("thai", nf.resolvedOptions().numberingSystem);
assertEquals("๑๒๓", nf.format(123));
assertEquals("en-u-nu-thai", nf.resolvedOptions().locale);

nf = new Intl.NumberFormat(["ar-EG-u-nu-latn"]);
assertEquals("latn", nf.resolvedOptions().numberingSystem);
assertEquals("123", nf.format(123));
assertEquals("ar-EG-u-nu-latn", nf.resolvedOptions().locale);
