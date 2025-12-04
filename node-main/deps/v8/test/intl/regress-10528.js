// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test date format in Hebrew calendar of dates prior to -003760-09-07
// On Android, hebrew calendar is only available on he locale.
let dateOK = new Date (Date.UTC(-3760, 8, 7));
let dateKO = new Date (Date.UTC(-3760, 8, 6));
let dateDisplay = new Intl.DateTimeFormat (
    'he-u-ca-hebrew',
    { timeZone : 'UTC', year : 'numeric', month :'long',
      day : 'numeric', weekday : 'long' });
assertEquals("Mon, 07 Sep -3760 00:00:00 GMT",
    dateOK.toUTCString(), "dateOK.toUTCString()");
assertEquals("Sun, 06 Sep -3760 00:00:00 GMT",
    dateKO.toUTCString(), "dateKO.toUTCString()");
assertEquals("יום שני, 1 בתשרי 1",
    dateDisplay.format(dateOK), "dateDisplay.format(dateOK)");
assertEquals("יום ראשון, 29 באלול 0",
    dateDisplay.format(dateKO), "dateDisplay.format(dateKO)");
