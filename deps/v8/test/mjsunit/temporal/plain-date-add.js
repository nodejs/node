// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

// Simple add
let d = new Temporal.PlainDate(2021, 7, 20);
assertPlainDate(d.add("P1D"), 2021, 7, 21);
assertPlainDate(d.subtract("-P1D"), 2021, 7, 21);
assertPlainDate(d.add("-P1D"), 2021, 7, 19);
assertPlainDate(d.subtract("P1D"), 2021, 7, 19);
assertPlainDate(d.add("P11D"), 2021, 7, 31);
assertPlainDate(d.subtract("-P11D"), 2021, 7, 31);
assertPlainDate(d.add("P12D"), 2021, 8, 1);
assertPlainDate(d.subtract("-P12D"), 2021, 8, 1);

let goodDate = new Temporal.PlainDate(2021, 7, 20);
let badDate = {add: goodDate.add};
assertThrows(() => badDate.add("P1D"), TypeError);

// Throw in ToLimitedTemporalDuration
assertThrows(() => (new Temporal.PlainDate(2021, 7, 20)).add("bad duration"),
    RangeError);
