// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

assertPlainDateTime(new Temporal.PlainDateTime(1, 2, 3, 4, 5, 6, 7, 8, 9),
    1, 2, 3, 4, 5, 6, 7, 8, 9);

assertPlainDateTime(new Temporal.PlainDateTime(1, 2, 3, 4, 5, 6, 7, 8),
    1, 2, 3, 4, 5, 6, 7, 8, 0);

assertPlainDateTime(new Temporal.PlainDateTime(1, 2, 3, 4, 5, 6, 7),
    1, 2, 3, 4, 5, 6, 7, 0, 0);

assertPlainDateTime(new Temporal.PlainDateTime(1, 2, 3, 4, 5, 6),
    1, 2, 3, 4, 5, 6, 0, 0, 0);

assertPlainDateTime(new Temporal.PlainDateTime(1, 2, 3, 4, 5),
    1, 2, 3, 4, 5, 0, 0, 0, 0);

assertPlainDateTime(new Temporal.PlainDateTime(1, 2, 3, 4),
    1, 2, 3, 4, 0, 0, 0, 0, 0);

assertPlainDateTime(new Temporal.PlainDateTime(1, 2, 3),
    1, 2, 3, 0, 0, 0, 0, 0, 0);

assertThrows(() => new Temporal.PlainDateTime(1, 2), RangeError);

assertThrows(() => new Temporal.PlainDateTime(1), RangeError);

assertThrows(() => new Temporal.PlainDateTime(), RangeError);

// smallest  values
assertPlainDateTime(new Temporal.PlainDateTime(-25406, 1, 1),
    -25406, 1, 1, 0, 0, 0, 0, 0, 0);

// largest  values
assertPlainDateTime(
    new Temporal.PlainDateTime(29345, 12, 31, 23, 59, 59, 999, 999, 999),
    29345, 12, 31, 23, 59, 59, 999, 999, 999);

assertPlainDateTime(
    new Temporal.PlainDateTime(false, true, true, NaN, undefined, true),
    0, 1, 1, 0, 0, 1, 0, 0, 0, 0);

assertPlainDateTime(
    new Temporal.PlainDateTime(11.9, 12.8, 13.7, 14.6, 15.5, 16.6, 17.7, 18.8,
      1.999999), 11, 12, 13, 14, 15, 16, 17, 18, 1);
