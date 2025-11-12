// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test in sr-ME locale, we consistently use Latin for both month and
// TimeZoneName.
let someCyrillicRE = /\p{Script=Cyrillic}/u;
let someLatinRE = /\p{Script=Latin}/u;
let allLatinRE = /[\s\d\.\p{Script=Latin}]+/u;
let allCyrillicRE = /^[\s\d\.\p{Script=Cyrillic}]+$/u;

let d = new Date();
let srMETimeZone = new Intl.DateTimeFormat("sr-ME",
    {timeZoneName: "long", timeZone: "America/Los_Angeles"});
let srMEMonth = new Intl.DateTimeFormat("sr-ME", {month: "long"});
let srTimeZone = new Intl.DateTimeFormat("sr",
    {timeZoneName: "long", timeZone: "America/Los_Angeles"});
let srMonth = new Intl.DateTimeFormat("sr", {month: "long"});

let srMETimeZoneString = srMETimeZone.format(d);
let srMEMonthString = srMEMonth.format(d);
let srTimeZoneString = srTimeZone.format(d);
let srMonthString = srMonth.format(d);

// sr-ME should have both in Latin
assertTrue(allLatinRE.test(srMETimeZoneString), srMETimeZoneString);
assertTrue(allLatinRE.test(srMEMonthString), srMEMonthString);
assertFalse(someCyrillicRE.test(srMETimeZoneString), srMETimeZoneString);
assertFalse(someCyrillicRE.test(srMEMonthString), srMEMonthString);

// sr should have both in Cyrillic
assertTrue(allCyrillicRE.test(srTimeZoneString), srTimeZoneString);
assertTrue(allCyrillicRE.test(srMonthString), srMonthString);
assertFalse(someLatinRE.test(srTimeZoneString), srTimeZoneString);
assertFalse(someLatinRE.test(srMonthString), srMonthString);
