// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-intl-locale-info-func

function checkLocale(locale) {
  let l = new Intl.Locale(locale);
  assertTrue(Array.isArray(l.getCalendars()));
  assertTrue(Array.isArray(l.getCollations()));
  assertTrue(Array.isArray(l.getHourCycles()));
  assertTrue(Array.isArray(l.getNumberingSystems()));

  if (l.region == undefined) {
    assertEquals(undefined, l.getTimeZones());
  } else {
    assertTrue(Array.isArray(l.getTimeZones()));
  }

  assertEquals("object", typeof(l.getTextInfo()));
  assertEquals(1, Object.keys(l.getTextInfo()).length);
  assertEquals("string", typeof(l.getTextInfo().direction));

  assertEquals("object", typeof(l.getWeekInfo()));
  assertEquals(2, Object.keys(l.getWeekInfo()).length);
  assertEquals("number", typeof(l.getWeekInfo().firstDay));
  assertTrue(l.getWeekInfo().firstDay >= 1);
  assertTrue(l.getWeekInfo().firstDay <= 7);

  assertEquals("object", typeof(l.getWeekInfo().weekend));
  let last = 0;
  l.getWeekInfo().weekend.forEach((we) => {
    // In right range
    assertTrue(we >= 1);
    assertTrue(we <= 7);
    // In order
    assertTrue(we >= last);
    last = we;
  });

  // The proposal removed minimalDays in PR99
  assertEquals(undefined, l.getWeekInfo().minimalDays);
}

checkLocale("ar");
checkLocale("en");
checkLocale("fr");
checkLocale("en-GB");
