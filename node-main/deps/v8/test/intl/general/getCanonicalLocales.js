// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ignore the first tag when checking for duplicate subtags.
assertDoesNotThrow(() => Intl.getCanonicalLocales("foobar-foobar"));

// Ignore duplicate subtags in different namespaces; eg, 'a' vs 'u'.
assertDoesNotThrow(() => Intl.getCanonicalLocales("en-a-ca-Chinese-u-ca-Chinese"));
// Ignore duplicate subtags in U-extension as well. Only the first count.
// See RFC 6067 for details.
assertDoesNotThrow(() => Intl.getCanonicalLocales("en-u-ca-gregory-ca-chinese"));
assertEquals("en-u-ca-gregory", Intl.getCanonicalLocales("en-u-ca-gregory-ca-chinese")[0]);

// Check duplicate subtags (after the first tag) are detected.
assertThrows(() => Intl.getCanonicalLocales("en-foobar-foobar"), RangeError);

// Check some common case
assertEquals("id", Intl.getCanonicalLocales("in")[0]);
assertEquals("he", Intl.getCanonicalLocales("iw")[0]);
assertEquals("yi", Intl.getCanonicalLocales("ji")[0]);
assertEquals("jv", Intl.getCanonicalLocales("jw")[0]);
assertEquals("ro", Intl.getCanonicalLocales("mo")[0]);
assertEquals("sr", Intl.getCanonicalLocales("scc")[0]);
assertEquals("sr-Latn", Intl.getCanonicalLocales("sh")[0]);
assertEquals("sr-ME", Intl.getCanonicalLocales("cnr")[0]);
assertEquals("no", Intl.getCanonicalLocales("no")[0]);
assertEquals("fil", Intl.getCanonicalLocales("tl")[0]);
