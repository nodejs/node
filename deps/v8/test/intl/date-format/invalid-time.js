// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var df = new Intl.DateTimeFormat();

assertThrows("df.format(Infinity)", RangeError);
assertThrows("df.formatToParts(Infinity)", RangeError);
assertThrows("df.format(-Infinity)", RangeError);
assertThrows("df.formatToParts(-Infinity)", RangeError);
assertThrows("df.format(NaN)", RangeError);
assertThrows("df.formatToParts(NaN)", RangeError);

// https://crbug.com/774833
var df2 = new Intl.DateTimeFormat('en', {'hour': 'numeric'});
Date.prototype.valueOf = "ponies";
assertEquals(df.format(Date.now()), df.format());
assertEquals(df2.format(Date.now()), df2.format());
assertEquals(df.formatToParts(Date.now()), df.formatToParts());
assertEquals(df2.formatToParts(Date.now()), df2.formatToParts());
