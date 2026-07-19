// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test date format in islamic-umalqura calendar of date prior to
// -195366-07-23
// On Android, islamic-umalqura calendar is only available on ar and fa locales.
let dateOK = new Date (Date.UTC(-195366, 6, 23));
let dateKO = new Date (Date.UTC(-195366, 6, 22));
let dateDisplay = new Intl.DateTimeFormat (
    'ar-u-ca-islamic-umalqura',
    { timeZone : 'UTC', year : 'numeric', month :'long',
      day : 'numeric', weekday : 'long' });
assertEquals("Wed, 23 Jul -195366 00:00:00 GMT",
    dateOK.toUTCString(), "dateOK.toUTCString()");
assertEquals("Tue, 22 Jul -195366 00:00:00 GMT",
    dateKO.toUTCString(), "dateKO.toUTCString()");
assertEquals("الأربعاء، 17 ذو الحجة ‎-202003 هـ",
    dateDisplay.format(dateOK), "dateDisplay.format(dateOK)");
assertEquals("الثلاثاء، 16 ذو الحجة ‎-202003 هـ",
    dateDisplay.format(dateKO), "dateDisplay.format(dateKO)");
