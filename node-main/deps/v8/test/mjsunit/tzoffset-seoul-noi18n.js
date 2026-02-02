// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-icu-timezone-data
// Environment Variables: TZ=Asia/Seoul

// Seoul has DST (UTC+10) in 1987 and 1988.
assertEquals(new Date(Date.UTC(1986, 5, 22, 3)),
   new Date(1986, 5, 22, 12))
assertEquals(new Date(Date.UTC(1987, 5, 22, 2)),
   new Date(1987, 5, 22, 12))
assertEquals(new Date(Date.UTC(1987, 11, 22, 3)),
   new Date(1987, 11, 22, 12))
assertEquals(new Date(Date.UTC(1988, 5, 22, 2)),
   new Date(1988, 5, 22, 12))
assertEquals(new Date(Date.UTC(1988, 11, 22, 3)),
   new Date(1988, 11, 22, 12))
assertEquals(new Date(Date.UTC(1989, 5, 22, 3)),
   new Date(1989, 5, 22, 12))
