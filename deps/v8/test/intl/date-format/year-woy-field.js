// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test DateTimeFormat with locale patterns using Week-of-Year Year ('Y')
// such as Telugu with Buddhist calendar ("te-u-ca-buddhist").

let dtf = new Intl.DateTimeFormat("te-u-ca-buddhist", {
  year: "numeric",
  month: "long"
});

// 1. Verify resolvedOptions includes 'year' property.
let options = dtf.resolvedOptions();
assertEquals("numeric", options.year);
assertEquals("long", options.month);
assertEquals("buddhist", options.calendar);

// 2. Verify formatToParts does not crash and returns the year part.
let parts = dtf.formatToParts(new Date(2026, 7, 31));
assertTrue(parts.some(part => part.type === "year"));
assertTrue(parts.some(part => part.type === "month"));

// 3. Verify formatRangeToParts does not crash and includes the year part.
let rangeParts = dtf.formatRangeToParts(new Date(2020, 0, 1), new Date(2021, 0, 1));
assertTrue(rangeParts.some(part => part.type === "year"));
