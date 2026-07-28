// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-intl-locale-info-func

// Test Unicode extension.
function testExt(base, ukey, uvalue, property) {
  let baseLocale = new Intl.Locale(base);
  let extLocale = new Intl.Locale(base + "-u-" + ukey + "-" + uvalue);

  let baseActual = baseLocale[property]();
  assertTrue(Array.isArray(baseActual));
  assertTrue(baseActual.length > 0);
  assertFalse(uvalue == baseActual[0]);

  let extActual = extLocale[property]();
  assertTrue(Array.isArray(extActual));
  assertEquals(1, extActual.length);
  assertEquals(uvalue, extActual[0]);
}

testExt("ar", "ca", "roc", "getCalendars");
testExt("fr", "nu", "thai", "getNumberingSystems");
testExt("th", "co", "pinyin", "getCollations");
testExt("ko", "hc", "h11", "getHourCycles");
