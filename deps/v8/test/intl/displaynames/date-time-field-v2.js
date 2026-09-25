// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const dateTimeFields = [
    'era', 'year', 'quarter', 'month', 'weekOfYear', 'weekday', 'day',
    'dayPeriod', 'hour', 'minute', 'second', 'timeZoneName'
];
let en = new Intl.DisplayNames("en", {type: 'dateTimeField'});
let zh = new Intl.DisplayNames("zh", {type: 'dateTimeField'});

dateTimeFields.forEach(function(dateTimeField) {
    assertFalse(en.of(dateTimeField) == zh.of(dateTimeField),
        dateTimeField + ":" + en.of(dateTimeField) + " == " +
        zh.of(dateTimeField));
});
