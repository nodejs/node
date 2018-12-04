// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-relative-time-format

// Check plural w/ formatToParts
// http://tc39.github.io/proposal-intl-relative-time/

let rtf = new Intl.RelativeTimeFormat();

// Test 1.4.4 Intl.RelativeTimeFormat.prototype.formatToParts( value, unit )
function verifyElement(part, expectedUnit) {
  assertEquals(true, part.type == 'literal' || part.type == 'integer');
  assertEquals('string', typeof part.value);
  if (part.type == 'integer') {
    assertEquals('string', typeof part.unit);
    assertEquals(expectedUnit, part.unit);
  }
};

['year', 'quarter', 'month', 'week', 'day', 'hour', 'minute', 'second'].forEach(
    function(unit) {
      rtf.formatToParts(100, unit + 's').forEach(
          function(part) {
            verifyElement(part, unit);
          });
    });
