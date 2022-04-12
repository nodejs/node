// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test Unicode extension.
function testExt(base, ukey, uvalue, property) {
  let baseLocale = new Intl.Locale(base);
  let extLocale = new Intl.Locale(base + "-u-" + ukey + "-" + uvalue);

  let baseActual = baseLocale[property];
  assertTrue(Array.isArray(baseActual));
  assertTrue(baseActual.length > 0);
  assertFalse(uvalue == baseActual[0]);

  let extActual = extLocale[property];
  assertTrue(Array.isArray(extActual));
  assertEquals(1, extActual.length);
  assertEquals(uvalue, extActual[0]);
}

testExt("ar", "ca", "roc", "calendars");
testExt("fr", "nu", "thai", "numberingSystems");
testExt("th", "co", "pinyin", "collations");
testExt("ko", "hc", "h11", "hourCycles");
