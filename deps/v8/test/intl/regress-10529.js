// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test date format in Indian calendar of date prior to 0001-01-01
// On Android, indian calendar is only available on hi locale.
let dateOK = new Date ("0001-12-31T23:52:58.000Z");
let dateKO = new Date ("0000-12-30T23:52:58.000Z");
let dateDisplay = new Intl.DateTimeFormat (
    'hi-u-ca-indian',
    { timeZone : 'UTC', year : 'numeric', month :'long',
      day : 'numeric', weekday : 'long' });
assertEquals("Mon, 31 Dec 0001 23:52:58 GMT",
    dateOK.toUTCString(), "dateOK.toUTCString()");
assertEquals("Sat, 30 Dec 0000 23:52:58 GMT",
    dateKO.toUTCString(), "dateKO.toUTCString()");
assertEquals("शक -77 पौष 10, सोमवार",
    dateDisplay.format(dateOK), "dateDisplay.format(dateOK)");
assertEquals("शक -78 पौष 9, शनिवार",
    dateDisplay.format(dateKO), "dateDisplay.format(dateKO)");
