// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-timezone-data
// Environment Variables: TZ=Europe/Moscow

// https://www.timeanddate.com/time/zone/russia/moscow

// 2010-03-28T02:00 : UTC+3 => UTC+4
assertEquals(new Date(Date.UTC(2010, 2, 27, 22, 59)),
   new Date(2010, 2, 28, 1, 59));
assertEquals(new Date(Date.UTC(2010, 2, 27, 23)),
   new Date(2010, 2, 28, 2));
assertEquals(new Date(Date.UTC(2010, 2, 27, 23, 30)),
   new Date(2010, 2, 28, 2, 30));
assertEquals(new Date(Date.UTC(2010, 2, 27, 23)),
   new Date(2010, 2, 28, 3));
assertEquals(new Date(Date.UTC(2010, 2, 27, 23, 30)),
   new Date(2010, 2, 28, 3, 30));
assertEquals((new Date(2010, 2, 28, 3, 30)).getTimezoneOffset(),
   (new Date(2010, 2, 28, 2, 30)).getTimezoneOffset());

// 2010-10-31T03:00 : UTC+4 => UTC+3
assertEquals(new Date(Date.UTC(2010, 9, 30, 21, 59)),
   new Date(2010, 9, 31, 1, 59));
assertEquals(new Date(Date.UTC(2010, 9, 30, 22)),
   new Date(2010, 9, 31, 2));
assertEquals(new Date(Date.UTC(2010, 9, 30, 22, 30)),
   new Date(2010, 9, 31, 2, 30));
assertEquals(new Date(Date.UTC(2010, 9, 30, 22, 59)),
   new Date(2010, 9, 31, 2, 59))
assertEquals(new Date(Date.UTC(2010, 9, 31, 0)),
   new Date(2010, 9, 31, 3))
assertEquals(new Date(Date.UTC(2010, 9, 31, 0, 30)),
   new Date(2010, 9, 31, 3, 30))

// 2011-03-27T02:00 : UTC+3 => UTC+4
assertEquals(new Date(Date.UTC(2011, 2, 26, 22, 59)),
   new Date(2011, 2, 27, 1, 59))
assertEquals(new Date(Date.UTC(2011, 2, 26, 23)),
   new Date(2011, 2, 27, 2));
assertEquals(new Date(Date.UTC(2011, 2, 26, 23, 30)),
   new Date(2011, 2, 27, 2, 30));
assertEquals(new Date(Date.UTC(2011, 2, 26, 23)),
   new Date(2011, 2, 27, 3));
assertEquals(new Date(Date.UTC(2011, 2, 26, 23, 30)),
   new Date(2011, 2, 27, 3, 30));
assertEquals((new Date(2011, 2, 27, 3, 30)).getTimezoneOffset(),
   (new Date(2011, 2, 27, 2, 30)).getTimezoneOffset());

// No daylight saving time in 2012, 2013: UTC+4 year-round
assertEquals(new Date(Date.UTC(2012, 5, 21, 0)),
   new Date(2012, 5, 21, 4))
assertEquals(new Date(Date.UTC(2012, 11, 21, 0)),
   new Date(2012, 11, 21, 4))
assertEquals(new Date(Date.UTC(2013, 5, 21, 0)),
   new Date(2013, 5, 21, 4))
assertEquals(new Date(Date.UTC(2013, 11, 21, 0)),
   new Date(2013, 11, 21, 4))

// 2014-10-26T0200: UTC+4 => UTC+3 (year-round)
assertEquals(new Date(Date.UTC(2014, 9, 25, 20, 59)),
   new Date(2014, 9, 26, 0, 59));
assertEquals(new Date(Date.UTC(2014, 9, 25, 21)),
   new Date(2014, 9, 26, 1));
assertEquals(new Date(Date.UTC(2014, 9, 25, 21, 30)),
   new Date(2014, 9, 26, 1, 30));
assertEquals(new Date(Date.UTC(2014, 9, 25, 21, 59)),
   new Date(2014, 9, 26, 1, 59))
assertEquals(new Date(Date.UTC(2014, 9, 25, 23)),
   new Date(2014, 9, 26, 2))
assertEquals(new Date(Date.UTC(2014, 9, 25, 23, 1)),
   new Date(2014, 9, 26, 2, 1))

assertEquals(new Date(Date.UTC(2014, 11, 21, 0)),
   new Date(2014, 11, 21, 3))
assertEquals(new Date(Date.UTC(2015, 5, 21, 0)),
   new Date(2015, 5, 21, 3))
assertEquals(new Date(Date.UTC(2015, 11, 21, 0)),
   new Date(2015, 11, 21, 3))
assertEquals(new Date(Date.UTC(2016, 5, 21, 0)),
   new Date(2016, 5, 21, 3))
assertEquals(new Date(Date.UTC(2015, 11, 21, 0)),
   new Date(2015, 11, 21, 3))
