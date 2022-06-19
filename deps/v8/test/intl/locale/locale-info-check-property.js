// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check getter properties against the spec.
function checkProperties(property) {
  let desc = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, property);
  assertEquals(`get ${property}`, desc.get.name);
  assertEquals('function', typeof desc.get)
  assertEquals(undefined, desc.set);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
}

checkProperties('calendars');
checkProperties('collations');
checkProperties('hourCycles');
checkProperties('numberingSystems');
checkProperties('textInfo');
checkProperties('timeZones');
checkProperties('weekInfo');
