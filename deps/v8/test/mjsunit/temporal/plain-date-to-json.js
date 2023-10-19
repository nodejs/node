// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// years in 4 digits range
assertEquals("2021-07-01", (new Temporal.PlainDate(2021, 7, 1)).toJSON());
assertEquals("9999-12-31", (new Temporal.PlainDate(9999, 12, 31)).toJSON());
assertEquals("1000-01-01", (new Temporal.PlainDate(1000, 1, 1)).toJSON());
assertEquals("0099-08-01", (new Temporal.PlainDate(99, 8, 1)).toJSON());
assertEquals("0999-12-31", (new Temporal.PlainDate(999, 12, 31)).toJSON());

// years out of 4 digits range
assertEquals("+010000-01-01", (new Temporal.PlainDate(10000, 1, 1)).toJSON());
assertEquals("+025021-07-01", (new Temporal.PlainDate(25021, 7, 1)).toJSON());
assertEquals("-000020-09-30", (new Temporal.PlainDate(-20, 9, 30)).toJSON());
assertEquals("-002021-07-01", (new Temporal.PlainDate(-2021, 7, 1)).toJSON());
assertEquals("-022021-07-01", (new Temporal.PlainDate(-22021, 7, 1)).toJSON());
