// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-timezone-data
// Environment Variables: TZ=Australia/Lord_Howe LC_ALL=en

assertEquals(
   "Mon Jan 01 1990 11:00:00 GMT+1100 (Lord Howe Daylight Time)",
   new Date("1990-01-01").toString());

assertEquals(
   "Fri Jun 01 1990 10:30:00 GMT+1030 (Lord Howe Standard Time)",
   new Date("1990-06-01").toString());
