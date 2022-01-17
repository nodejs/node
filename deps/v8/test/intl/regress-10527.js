// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test date format in  Islamic and Islamic-rgsa calendars of dates prior
// to 0622-07-18
// On Android, islamic and islamic-rgsac calendar are only available on ar
// and fa locales.
let dateOK = new Date (Date.UTC(622, 6, 18));
let dateKO = new Date (Date.UTC(622, 6, 17));
let dateDisplay = new Intl.DateTimeFormat (
    'ar-u-ca-islamic',
    { timeZone : 'UTC', year : 'numeric', month :'long',
      day : 'numeric', weekday : 'long' });
let dateDisplay2 = new Intl.DateTimeFormat (
    'ar-u-ca-islamic-rgsa',
    { timeZone : 'UTC', year : 'numeric', month :'long',
      day : 'numeric', weekday : 'long' });
assertEquals("Thu, 18 Jul 0622 00:00:00 GMT",
    dateOK.toUTCString(), "dateOK.toUTCString()");
assertEquals("Wed, 17 Jul 0622 00:00:00 GMT",
    dateKO.toUTCString(), "dateKO.toUTCString()");
assertEquals("الخميس، 1 محرم 1 هـ",
    dateDisplay.format(dateOK), "dateDisplay.format(dateOK)");
assertEquals("الأربعاء، 30 ذو الحجة 0 هـ",
    dateDisplay.format(dateKO), "dateDisplay.format(dateKO)");
assertEquals("الخميس، 1 محرم 1 هـ",
    dateDisplay2.format(dateOK), "dateDisplay.format(dateOK)");
assertEquals("الأربعاء، 30 ذو الحجة 0 هـ",
    dateDisplay2.format(dateKO), "dateDisplay.format(dateKO)");
