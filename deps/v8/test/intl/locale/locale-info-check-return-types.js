// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  assertEquals(3, Object.keys(l.weekInfo).length);
  assertEquals("number", typeof(l.weekInfo.firstDay));
  assertTrue(l.weekInfo.firstDay >= 1);
  assertTrue(l.weekInfo.firstDay <= 7);

  assertEquals("object", typeof(l.weekInfo.weekend));
  let last = 0;
  l.weekInfo.weekend.forEach((we) => {
    // In right range
    assertTrue(we >= 1);
    assertTrue(we <= 7);
    // In order
    assertTrue(we >= last);
    last = we;
  });

  assertEquals("number", typeof(l.weekInfo.minimalDays));
  assertTrue(l.weekInfo.minimalDays >= 1);
  assertTrue(l.weekInfo.minimalDays <= 7);
}

checkLocale("ar");
checkLocale("en");
checkLocale("fr");
checkLocale("en-GB");
