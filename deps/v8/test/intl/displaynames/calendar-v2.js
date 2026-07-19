// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.islamic'

const calendars = [
    'buddhist', 'chinese', 'coptic', 'dangi', 'ethioaa', 'ethiopic-amete-alem',
    'ethiopic', 'gregory', 'hebrew', 'indian', 'islamic', 'islamic-umalqura',
    'islamic-tbla', 'islamic-civil', 'islamic-rgsa', 'iso8601', 'japanese',
    'persian', 'roc'
];
let en = new Intl.DisplayNames("en", {type: 'calendar'});
let zh = new Intl.DisplayNames("zh", {type: 'calendar'});

calendars.forEach(function(calendar) {
    assertFalse(en.of(calendar) == zh.of(calendar),
        calendar + ":" + en.of(calendar) + " == " +
        zh.of(calendar));
});
