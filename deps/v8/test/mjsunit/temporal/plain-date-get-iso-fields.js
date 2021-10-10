// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDate(1911, 10, 10);
assertPlainDate(d1, 1911, 10, 10);
let d2 = new Temporal.PlainDate(2020, 3, 12);
assertPlainDate(d2, 2020, 3, 12);
let d3 = new Temporal.PlainDate(1, 12, 25);
assertPlainDate(d3, 1, 12, 25);
let d4 = new Temporal.PlainDate(1970, 1, 1);
assertPlainDate(d4, 1970, 1, 1);
let d5 = new Temporal.PlainDate(-10, 12, 1);
assertPlainDate(d5, -10, 12, 1);
let d6 = new Temporal.PlainDate(-25406, 1, 1);
assertPlainDate(d6, -25406, 1, 1);
let d7 = new Temporal.PlainDate(26890, 12, 31);
assertPlainDate(d7, 26890, 12, 31);
