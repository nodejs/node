// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let d1 = new Temporal.PlainDate(2021, 2, 28);
let d2 = Temporal.PlainDate.from("2021-02-28");
let d3 = Temporal.PlainDate.from("2021-01-28");

assertEquals(d1.equals(d2), true);
assertEquals(d1.equals(d3), false);
assertEquals(d2.equals(d3), false);

let badDate = {equals: d1.equals};
assertThrows(() => badDate.equals(d1), TypeError);
