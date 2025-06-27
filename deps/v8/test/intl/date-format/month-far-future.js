// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for crbug.com/801602 .

var locales = [
    "en-u-ca-gregori",
    "fa-u-ca-persian",
    "ar-u-ca-islamic-civil",
    "ar-u-ca-islamic-umalqura",
    "ar-u-ca-islamic-tbla",
    "ar-u-ca-islamic-rgsa",
    "he-u-ca-hebrew",
    "zh-u-ca-chinese",
    "ko-u-ca-dangi",
    "ja-u-ca-japanese",
    "am-u-ca-ethiopic",
    "am-u-ca-ethioaa",
    "hi-u-ca-indian",
    "th-u-ca-buddhist",
];

// Used to test with 1.7976931348623157e+308, but it does not work
// any more with TimeClip. Instead, try the largest time value.
var end_of_time =  8.64e15;

locales.forEach(function(loc) {
  var df = new Intl.DateTimeFormat(loc, {month: "long"});
  assertFalse(df.format(end_of_time) == '');
}
);
