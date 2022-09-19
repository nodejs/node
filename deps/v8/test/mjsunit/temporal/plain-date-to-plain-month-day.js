// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDate(2021, 12, 11);
let badDateTime = { toPlainMonthDay: d1.toPlainMonthDay }
assertThrows(() => badDateTime.toPlainMonthDay(), TypeError);

assertPlainMonthDay(d1.toPlainMonthDay(), "M12", 11);
