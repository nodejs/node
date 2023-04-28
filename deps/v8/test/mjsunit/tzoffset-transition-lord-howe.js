// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-timezone-data
// Environment Variables: TZ=Australia/Lord_Howe


// 2017-04-02T02:00 : UTC+11 => UTC+1030
assertEquals(new Date(Date.UTC(2017, 3, 1, 14, 29)),
   new Date(2017, 3, 2, 1, 29));
assertEquals(new Date(Date.UTC(2017, 3, 1, 14, 30)),
   new Date(2017, 3, 2, 1, 30));
assertEquals(new Date(Date.UTC(2017, 3, 1, 14, 45)),
   new Date(2017, 3, 2, 1, 45));
assertEquals(new Date(Date.UTC(2017, 3, 1, 14, 59)),
   new Date(2017, 3, 2, 1, 59));
assertEquals(new Date(Date.UTC(2017, 3, 1, 15, 30)),
   new Date(2017, 3, 2, 2));
assertEquals(new Date(Date.UTC(2017, 3, 1, 15, 31)),
   new Date(2017, 3, 2, 2, 1));

// 2017-10-07T02:00 : UTC+1030 => UTC+11
assertEquals(new Date(Date.UTC(2017, 8, 30, 15, 29)),
   new Date(2017, 9, 1, 1, 59))
assertEquals(new Date(Date.UTC(2017, 8, 30, 15, 30)),
   new Date(2017, 9, 1, 2));
assertEquals(new Date(Date.UTC(2017, 8, 30, 15, 45)),
   new Date(2017, 9, 1, 2, 15));
assertEquals(new Date(Date.UTC(2017, 8, 30, 15, 30)),
   new Date(2017, 9, 1, 2, 30));
assertEquals(new Date(Date.UTC(2017, 8, 30, 15, 45)),
   new Date(2017, 9, 1, 2, 45));
assertEquals((new Date(2017, 9, 1, 2, 45)).getTimezoneOffset(),
   (new Date(2017, 9, 1, 2, 15)).getTimezoneOffset());
