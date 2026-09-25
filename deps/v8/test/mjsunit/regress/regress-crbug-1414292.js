// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const date = new Date("Wed Feb 15 2023 00:00:00 GMT+0100");
const localeString = date.toLocaleString("en-US");
// No narrow-width space should be found
assertEquals(-1, localeString.search('\u202f'));
// Regular space should match the character between time and AM/PM.
assertMatches(/:\d\d:\d\d [AP]M$/, localeString);

const formatter = new Intl.DateTimeFormat('en', {timeStyle: "long"})
const formattedString = formatter.format(date)
// No narrow-width space should be found
assertEquals(-1, formattedString.search('\u202f'));
// Regular space should match the character between time and AM/PM.
assertMatches(/:\d\d:\d\d [AP]M$/, localeString);
