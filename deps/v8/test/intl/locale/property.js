// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-locale

// Make sure that accessing locale property will return undefined instead of
// crash.

let locale = new Intl.Locale('sr');

assertEquals('sr', locale.toString());
assertEquals('sr', locale.baseName);
assertEquals('sr', locale.language);
assertEquals(undefined, locale.script);
assertEquals(undefined, locale.region);
assertEquals(false, locale.numeric);
assertEquals(undefined, locale.calendar);
assertEquals(undefined, locale.collation);
assertEquals(undefined, locale.hourCycle);
assertEquals(undefined, locale.caseFirst);
assertEquals(undefined, locale.numberingSystem);
