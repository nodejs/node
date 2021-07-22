// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony_intl_locale_info

function checkLocale(locale) {
  let l = new Intl.Locale(locale);
  assertTrue(Array.isArray(l.calendars));
  assertTrue(Array.isArray(l.collations));
  assertTrue(Array.isArray(l.hourCycles));
  assertTrue(Array.isArray(l.numberingSystems));

  if (l.region == undefined) {
    assertEquals(undefined, l.timeZones);
  } else {
    assertTrue(Array.isArray(l.timeZones));
  }

  assertEquals("object", typeof(l.textInfo));
  assertEquals(1, Object.keys(l.textInfo).length);
  assertEquals("string", typeof(l.textInfo.direction));

  assertEquals("object", typeof(l.weekInfo));
  assertEquals(4, Object.keys(l.weekInfo).length);
  assertEquals("number", typeof(l.weekInfo.firstDay));
  assertTrue(l.weekInfo.firstDay >= 1);
  assertTrue(l.weekInfo.firstDay <= 7);

  assertEquals("number", typeof(l.weekInfo.weekendStart));
  assertTrue(l.weekInfo.weekendStart >= 1);
  assertTrue(l.weekInfo.weekendStart <= 7);

  assertEquals("number", typeof(l.weekInfo.weekendEnd));
  assertTrue(l.weekInfo.weekendEnd >= 1);
  assertTrue(l.weekInfo.weekendEnd <= 7);

  assertEquals("number", typeof(l.weekInfo.minimalDays));
  assertTrue(l.weekInfo.minimalDays >= 1);
  assertTrue(l.weekInfo.minimalDays <= 7);
}

checkLocale("ar");
checkLocale("en");
checkLocale("fr");
checkLocale("en-GB");
