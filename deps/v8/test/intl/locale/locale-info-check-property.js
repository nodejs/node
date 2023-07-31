// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-intl-locale-info-func
// Check function properties against the spec.
function checkProperties(property) {
  let desc = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, property);
  assertEquals('function', typeof desc.value)
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertTrue(desc.writable);
}

checkProperties('getCalendars');
checkProperties('getCollations');
checkProperties('getHourCycles');
checkProperties('getNumberingSystems');
checkProperties('getTextInfo');
checkProperties('getTimeZones');
checkProperties('getWeekInfo');
