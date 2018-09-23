// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var date = new Date("2016/01/02 10:00 GMT-8")
assertEquals(0, date.getMinutes());
assertEquals(18, date.getUTCHours());

date = new Date("2016/01/02 10:00 GMT-12")
assertEquals(0, date.getMinutes());
assertEquals(22, date.getUTCHours());

date = new Date("2016/01/02 10:00 GMT-123")
assertEquals(23, date.getMinutes());
assertEquals(11, date.getUTCHours());

date = new Date("2016/01/02 10:00 GMT-0856")
assertEquals(56, date.getMinutes());
assertEquals(18, date.getUTCHours());

date = new Date("2016/01/02 10:00 GMT-08000")
assertEquals(NaN, date.getMinutes());
assertEquals(NaN, date.getUTCHours());
