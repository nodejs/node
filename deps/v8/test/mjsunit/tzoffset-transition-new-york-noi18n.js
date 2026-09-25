// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-icu-timezone-data
// Environment Variables: TZ=America/New_York

// 2017-03-12T02:00 : UTC-5 => UTC-4
assertEquals(new Date(Date.UTC(2017, 2, 12, 6, 59)),
   new Date(2017, 2, 12, 1, 59))
assertEquals(new Date(Date.UTC(2017, 2, 12, 7)),
   new Date(2017, 2, 12, 2));
assertEquals(new Date(Date.UTC(2017, 2, 12, 7, 30)),
   new Date(2017, 2, 12, 2, 30));
assertEquals(new Date(Date.UTC(2017, 2, 12, 7)),
   new Date(2017, 2, 12, 3));
assertEquals(new Date(Date.UTC(2017, 2, 12, 7, 30)),
   new Date(2017, 2, 12, 3, 30));
assertEquals((new Date(2017, 2, 12, 3, 30)).getTimezoneOffset(),
   (new Date(2017, 2, 12, 2, 30)).getTimezoneOffset());

// 2017-11-05T02:00 : UTC-4 => UTC-5
assertEquals(new Date(Date.UTC(2017, 10, 5, 4, 59)),
   new Date(2017, 10, 5, 0, 59));
assertEquals(new Date(Date.UTC(2017, 10, 5, 5)),
   new Date(2017, 10, 5, 1));
assertEquals(new Date(Date.UTC(2017, 10, 5, 5, 30)),
   new Date(2017, 10, 5, 1, 30));
assertEquals(new Date(Date.UTC(2017, 10, 5, 5, 59)),
   new Date(2017, 10, 5, 1, 59));
assertEquals(new Date(Date.UTC(2017, 10, 5, 7)),
   new Date(2017, 10, 5, 2))
assertEquals(new Date(Date.UTC(2017, 10, 5, 8)),
   new Date(2017, 10, 5, 3))
