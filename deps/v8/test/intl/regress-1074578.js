// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the update of tz2020a
// https://mm.icann.org/pipermail/tz-announce/2020-April/000058.html
//    Morocco springs forward on 2020-05-31, not 2020-05-24.
//    Canada's Yukon advanced to -07 year-round on 2020-03-08.
//    America/Nuuk renamed from America/Godthab.
//    zic now supports expiration dates for leap second lists.

// A. Test Morocco springs forward on 2020-05-31, not 2020-05-24.

const df1 = new Intl.DateTimeFormat(
    "en",
    {timeZone: "Africa/Casablanca", timeStyle: "long", dateStyle: "long"})
const d1 = new Date("2020-05-25T00:00:00.000Z");
const d2 = new Date("2020-05-31T00:00:00.000Z");

// Before tz2020a change will get "May 25, 2020 at 1:00:00 AM GMT+1"
assertEquals("May 25, 2020 at 12:00:00 AM GMT", df1.format(d1));

// Before tz2020a change will get "May 31, 2020 at 1:00:00 AM GMT+1"
assertEquals("May 31, 2020 at 12:00:00 AM GMT", df1.format(d2));

// B. Test Canada's Yukon advanced to -07 year-round on 2020-03-08.
const df2 = new Intl.DateTimeFormat(
    "en",
    {timeZone: "Canada/Yukon", dateStyle: "long", timeStyle: "long"});
const d3 = new Date("2020-03-09T00:00Z");
const d4 = new Date("2021-03-09T00:00Z");

// Before tz202a change will get "March 8, 2020 at 5:00:00 PM PDT"
assertEquals("March 8, 2020 at 5:00:00 PM MST", df2.format(d3));

// Before tz202a change will get "March 8, 2021 at 4:00:00 PM PST"
assertEquals("March 8, 2021 at 5:00:00 PM MST", df2.format(d4));

// C. Test America/Nuuk renamed from America/Godthab.

// Before tz2020a will throw RangeError.
const df3 = new Intl.DateTimeFormat("en", {timeZone: "America/Nuuk"});

// Renamed timezone will return the stable name before the rename.
assertEquals("America/Godthab", df3.resolvedOptions().timeZone);
