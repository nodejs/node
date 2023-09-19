// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test invalid keys
["calendars", "collations", "currencies", "numberingSystems", "timeZones", "units",
 1, 0.3, true, false, {}, [] ].forEach(
    function(key) {
      assertThrows(() => Intl.supportedValuesOf(key), RangeError);
    });
