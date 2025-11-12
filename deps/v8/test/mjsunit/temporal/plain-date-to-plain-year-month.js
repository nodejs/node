// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

d8.file.execute('test/mjsunit/temporal/temporal-helpers.js');

let d1 = new Temporal.PlainDate(2021, 12, 11);
let badDate = { toPlainYearMonth: d1.toPlainYearMonth }
assertThrows(() => badDate.toPlainYearMonth(), TypeError);

assertPlainYearMonth(d1.toPlainYearMonth(), 2021, 12);
