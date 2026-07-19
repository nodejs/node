// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-timezone-data
// Environment Variables: TZ=Pacific/Apia

// https://www.timeanddate.com/time/zone/samoa/apia

// 2011-09-24T03:00 : UTC-11 => UTC-10
assertEquals(new Date(Date.UTC(2011, 8, 24, 13, 59)),
   new Date(2011, 8, 24, 2, 59))
assertEquals(new Date(Date.UTC(2011, 8, 24, 14, 30)),
   new Date(2011, 8, 24, 3, 30));
assertEquals(new Date(Date.UTC(2011, 8, 24, 14)),
   new Date(2011, 8, 24, 4));
assertEquals(new Date(Date.UTC(2011, 8, 24, 14, 30)),
   new Date(2011, 8, 24, 4, 30));
assertEquals((new Date(2011, 8, 24, 4, 30)).getTimezoneOffset(),
   (new Date(2011, 8, 24, 3, 30)).getTimezoneOffset());

// 2011-12-30T00:00 : UTC-10 => UTC+14
// A whole day(2011-12-30; 24 hours) is skipped, but the skipped
// time is to be interpreted with an offset before the transition.
assertEquals(new Date(Date.UTC(2011, 11, 30, 9, 59)),
   new Date(2011, 11, 29, 23, 59));
for (var h = 0; h < 24; ++h) {
  assertEquals(new Date(Date.UTC(2011, 11, 30, h + 10)),
     new Date(2011, 11, 30, h));
  assertEquals(new Date(Date.UTC(2011, 11, 30, h + 10, 30)),
     new Date(2011, 11, 30, h, 30));
  assertEquals(new Date(Date.UTC(2011, 11, 30, h + 10)),
     new Date(2011, 11, 31, h));
  assertEquals(new Date(Date.UTC(2011, 11, 30, h + 10, 30)),
     new Date(2011, 11, 31, h, 30));
}
assertEquals(new Date(Date.UTC(2011, 11, 31, 10)),
   new Date(2012, 0, 1, 0));

// 2012-04-01T0400: UTC+14 => UTC+13
assertEquals(new Date(Date.UTC(2012, 2, 31, 13)),
   new Date(2012, 3, 1, 3));
assertEquals(new Date(Date.UTC(2012, 2, 31, 13, 30)),
   new Date(2012, 3, 1, 3, 30));
assertEquals(new Date(Date.UTC(2012, 2, 31, 13, 59)),
   new Date(2012, 3, 1, 3, 59))
assertEquals(new Date(Date.UTC(2012, 2, 31, 15)),
   new Date(2012, 3, 1, 4))
