// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-timezone-data
// Environment Variables: TZ=Europe/Madrid LC_ALL=de

assertEquals(
   "Sun Dec 31 1989 00:00:00 GMT+0100 (Mitteleuropäische Normalzeit)",
   new Date(1990, 0, 0).toString());

assertEquals(
   "Sat Jun 30 1990 00:00:00 GMT+0200 (Mitteleuropäische Sommerzeit)",
   new Date(1990, 6, 0).toString());
