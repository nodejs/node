// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Intl) {
  // chromium:364374

  // Locations with 2 underscores are accepted and normalized.
  // 'of' and 'es' are always lowercased.
  df = new Intl.DateTimeFormat('en-US', {'timeZone': 'eUrope/isLe_OF_man'})
  assertEquals('Europe/Isle_of_Man', df.resolvedOptions().timeZone);

  df = new Intl.DateTimeFormat('en-US', {'timeZone': 'africa/Dar_eS_salaam'})
  assertEquals('Africa/Dar_es_Salaam', df.resolvedOptions().timeZone);

  df = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/port_of_spain'})
  assertEquals('America/Port_of_Spain', df.resolvedOptions().timeZone);

  // Zone ids with more than 2 parts are accepted and normalized.
  df = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/north_Dakota/new_salem'})
  assertEquals('America/North_Dakota/New_Salem', df.resolvedOptions().timeZone);

  // 3-part zone IDs are accepted and normalized.
  // Two Buenose Aires aliases are identical.
  df1 = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/aRgentina/buenos_aIres'})
  df2 = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/Argentina/Buenos_Aires'})
  assertEquals(df1.resolvedOptions().timeZone, df2.resolvedOptions().timeZone);

  df2 = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/Buenos_Aires'})
  assertEquals(df1.resolvedOptions().timeZone, df2.resolvedOptions().timeZone);

  df1 = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/Indiana/Indianapolis'})
  df2 = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/Indianapolis'})
  assertEquals(df1.resolvedOptions().timeZone, df2.resolvedOptions().timeZone);

  // ICU does not recognize East-Indiana. Add later when it does.
  // df2 = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/East-Indiana'})
  // assertEquals(df1.resolvedOptions().timeZone, df2.resolvedOptions().timeZone);


  // Zone IDs with hyphens. 'au' has to be in lowercase.
  df = new Intl.DateTimeFormat('en-US', {'timeZone': 'America/port-aU-pRince'})
  assertEquals('America/Port-au-Prince', df.resolvedOptions().timeZone);

  // Accepts Ho_Chi_Minh and treats it as identical to Saigon
  df1 = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Ho_Chi_Minh'})
  df2 = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Saigon'})
  assertEquals(df1.resolvedOptions().timeZone, df2.resolvedOptions().timeZone);

  // Throws for invalid timezone ids.
  assertThrows(() => Intl.DateTimeFormat(undefined, {timeZone: 'Europe/_Paris'}));
  assertThrows(() => Intl.DateTimeFormat(undefined, {timeZone: 'America/New__York'}));
  assertThrows(() => Intl.DateTimeFormat(undefined, {timeZone: 'America//New_York'}));
  assertThrows(() => Intl.DateTimeFormat(undefined, {timeZone: 'America/New_York_'}));
  assertThrows(() => Intl.DateTimeFormat(undefined, {timeZone: 'America/New_Y0rk'}));
}
