// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-icu-british-remove-full-weekday-comma

let date = new Date("2023-06-14T13:50Z");

function dateFormat(locale) {
    let f = new Intl.DateTimeFormat(locale, { dateStyle: "full", timeZone: "UTC" });
    return f.format(date);
}

// Locales that are affected by the flag
assertEquals("Wednesday, 14 June 2023", dateFormat("en-AU"));
assertEquals("Wednesday, 14 June 2023", dateFormat("en-GB"));
assertEquals("Wednesday, 14 June 2023", dateFormat("en-IN"));

// Locales that are not affected by the flag
assertEquals("Wednesday, June 14, 2023", dateFormat("en"));
assertEquals("Wednesday, June 14, 2023", dateFormat("en-CA"));
assertEquals("Wednesday, June 14, 2023", dateFormat("en-US"));
