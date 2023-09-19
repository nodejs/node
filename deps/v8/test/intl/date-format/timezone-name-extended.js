// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Tests time zone names.

// Winter date (PST).
var winter = new Date(2013, 1, 12, 14, 42, 53, 0);

// Summer date (PDT).
var summer = new Date(2013, 7, 12, 14, 42, 53, 0);

// Common flags for both formatters.
var flags = {
  year: 'numeric', month: 'long', day: 'numeric',
  hour : '2-digit', minute : '2-digit', second : '2-digit',
  timeZone: 'America/Los_Angeles'
};

flags.timeZoneName = "short";
var dfs = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dfs.format(winter).indexOf('PST') !== -1);
assertTrue(dfs.format(summer).indexOf('PDT') !== -1);
assertEquals(flags.timeZoneName, dfs.resolvedOptions().timeZoneName);

flags.timeZoneName = "long";
var dfl = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dfl.format(winter).indexOf('Pacific Standard Time') !== -1);
assertTrue(dfl.format(summer).indexOf('Pacific Daylight Time') !== -1);
assertEquals(flags.timeZoneName, dfl.resolvedOptions().timeZoneName);

flags.timeZoneName = "shortGeneric";
var dfsg = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dfsg.format(winter).indexOf('PT') !== -1);
assertTrue(dfsg.format(summer).indexOf('PT') !== -1);
assertEquals(flags.timeZoneName, dfsg.resolvedOptions().timeZoneName);

flags.timeZoneName = "longGeneric";
var dflg = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dflg.format(winter).indexOf('Pacific Time') !== -1);
assertTrue(dflg.format(summer).indexOf('Pacific Time') !== -1);
assertEquals(flags.timeZoneName, dflg.resolvedOptions().timeZoneName);

flags.timeZoneName = "shortOffset";
var dfso = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dfso.format(winter).indexOf('GMT-8') !== -1);
assertTrue(dfso.format(summer).indexOf('GMT-7') !== -1);
assertEquals(flags.timeZoneName, dfso.resolvedOptions().timeZoneName);

flags.timeZoneName = "longOffset";
var dflo = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dflo.format(winter).indexOf('GMT-08:00') !== -1);
assertTrue(dflo.format(summer).indexOf('GMT-07:00') !== -1);
assertEquals(flags.timeZoneName, dflo.resolvedOptions().timeZoneName);
