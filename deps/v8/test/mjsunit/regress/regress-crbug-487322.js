// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Intl) {
  // Normalizes Kat{h,}mandu (chromium:487322)
  // According to the IANA timezone db, Kathmandu is the current canonical
  // name, but ICU got it backward. To make this test robust against a future
  // ICU change ( http://bugs.icu-project.org/trac/ticket/12044 ),
  // just check that Kat(h)mandu is resolved identically.
  df1 = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Katmandu'})
  df2 = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Kathmandu'})
  assertEquals(df1.resolvedOptions().timeZone, df2.resolvedOptions().timeZone);

  // Normalizes Ulan_Bator to Ulaanbaatar. Unlike Kat(h)mandu, ICU got this
  // right so that we make sure that Ulan_Bator is resolved to Ulaanbaatar.
  df = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Ulaanbaatar'})
  assertEquals('Asia/Ulaanbaatar', df.resolvedOptions().timeZone);

  df = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Ulan_Bator'})
  assertEquals('Asia/Ulaanbaatar', df.resolvedOptions().timeZone);

  // Throws for unsupported time zones.
  assertThrows(() => Intl.DateTimeFormat(undefined, {timeZone: 'Aurope/Paris'}));
}
