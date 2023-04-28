// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the new Date( date.toLocaleString("en-US")) is not invalid.
// This is not guaranteed by the standard but many code use that to set the
// timezone as suggested in
// https://stackoverflow.com/questions/15141762/how-to-initialize-a-javascript-date-to-a-particular-time-zone

let d = new Date();

// https://tc39.es/ecma262/#sec-todatestring
// 21.4.4.41.4 ToDateString ( tv )
// 1. If tv is NaN, return "Invalid Date".
let invalid = "Invalid Date";
let largestDiff = 25*60*60*1000;

let garbage = new Date("garbage");
assertTrue(invalid == garbage);
assertEquals(NaN, garbage.getTime());

let d1 = new Date(d.toLocaleString("en-US"));
assertTrue(d1 != invalid);
assertTrue(d1.getTime() != NaN);
// The milliseconds are different between d1 and d.
assertTrue(Math.abs(d1-d) < 1000);

// Force a version of date string which have  U+202f before AM
let nnbsp_am = new Date("11/16/2022, 9:04:55\u202fAM");
assertTrue(nnbsp_am  != invalid);
assertTrue(nnbsp_am.getTime() != NaN);
// Force a version of date string which have  U+202f before PM
let nnbsp_pm = new Date("11/16/2022, 9:04:55\u202fPM");
assertTrue(nnbsp_pm  != invalid);
assertTrue(nnbsp_pm.getTime() != NaN);

let d2 = new Date(d.toLocaleString("en-US", {timeZone: "Asia/Taipei"}));
assertTrue(d2 != invalid);
assertTrue(d2.getTime() != NaN);
// The differences should be within 25 hours.
assertTrue(Math.abs(d2-d) < largestDiff);

let d3 = new Date(d.toLocaleString("en-US", {timeZone: "Africa/Lusaka"}));
assertTrue(d3 != invalid);
assertTrue(d3.getTime() != NaN);
// The differences should be within 25 hours.
assertTrue(Math.abs(d3-d) < largestDiff);
