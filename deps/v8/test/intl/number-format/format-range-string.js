// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the throw in formatRange

let df = new Intl.NumberFormat();

// https://tc39.es/proposal-intl-numberformat-v3/out/numberformat/diff.html#sec-partitionnumberrangepattern
// 2. If x is not-a-number or y is not-a-number, throw a RangeError exception.
assertThrows(() => { df.formatRange("xyz", "123") }, RangeError);
assertThrows(() => { df.formatRange("123", "xyz") }, RangeError);
assertThrows(() => { df.formatRange("1", "-0b1111") }, RangeError);
assertThrows(() => { df.formatRange("1", "-0o7654") }, RangeError);
assertThrows(() => { df.formatRange("1", "-0xabcde") }, RangeError);

assertDoesNotThrow(() => { df.formatRange(
   "  +1234567890123456789012345678901234567890123456789012345678901   ",
   "  +123456789012345678901234567890123456789012345678901234567890    ") });
assertDoesNotThrow(() => { df.formatRange(
   "  +123456789012345678901234567890.123456789012345678901234567890e25   ",
   "  +12345678901234567890.1234567890123456789012345678901234567890e25    ")});
assertDoesNotThrow(() => { df.formatRange(
   "  +12345678901234567890.1234567890123456789012345678901234567890e35   ",
   "  +123456789012345678901234567890.123456789012345678901234567890e24    ")});
assertDoesNotThrow(() => { df.formatRange(
   "  -123456789012345678901234567890123456789012345678901234567890    ",
   "  -1234567890123456789012345678901234567890123456789012345678901   ")});
assertDoesNotThrow(() => { df.formatRange(
   "  -12345678901234567890.1234567890123456789012345678901234567890e25   ",
   "  -123456789012345678901234567890.123456789012345678901234567890e25    ")});
assertDoesNotThrow(() => { df.formatRange(
   "  -123456789012345678901234567890.123456789012345678901234567890e24    ",
   "  -12345678901234567890.1234567890123456789012345678901234567890e35   ")});
assertDoesNotThrow(() => { df.formatRange(
   "  +.1234567890123456789012345678901234567890123456789012345678901   ",
   "  +.123456789012345678901234567890123456789012345678901234567890    ")});
assertDoesNotThrow(() => { df.formatRange(
   "  +.123456789012345678901234567890123456789012345678901234567890   ",
   "  -.1234567890123456789012345678901234567890123456789012345678901    ")});
assertDoesNotThrow(() => { df.formatRange(
   "  +.12e3   ", "  +.12e2    ") });
assertDoesNotThrow(() => { df.formatRange(
   "  +123   ", "  +.12e2    ") });
assertDoesNotThrow(() => { df.formatRange(
   "  -123   ", "  -.12e4    ") });

assertDoesNotThrow(() => { df.formatRange( "  123   ", "  -Infinity    ")});
assertDoesNotThrow(() => { df.formatRange( "  123   ", "  -0    ")});

// other case which won't throw
assertDoesNotThrow(() => { df.formatRange( "  123   ", "  Infinity    ") })
assertEquals("123–∞", df.formatRange( "  123   ", "  Infinity    "));
assertDoesNotThrow(() => { df.formatRange(
    "   +.123456789012345678901234567890123456789012345678901234567890   ", "  Infinity    ") })
assertEquals("0.123–∞", df.formatRange(
    "   +.123456789012345678901234567890123456789012345678901234567890   ",
    "  Infinity    "));
assertDoesNotThrow(() => { df.formatRange(
    "   +.123456789012345678901234567890123456789012345678901234567890   ",
    "   +.123456789012345678901234567890123456789012345678901234567890  ")})
assertDoesNotThrow(() => { df.formatRange(
    "   +.123456789012345678901234567890123456789012345678901234567890   ",
    "   +.1234567890123456789012345678901234567890123456789012345678901  ")})
assertDoesNotThrow(() => { df.formatRange(
    "   +12345678901234567890.123456789012345678901234567890123456789000000001e20   ",
    "   +1234567890.12345678901234567890123456789012345678901234567890e31  ")})
assertDoesNotThrow(() => { df.formatRange( "  Infinity   ", "  123    ")});
assertDoesNotThrow(() => { df.formatRange( "  +Infinity   ", "  123    ")});
assertDoesNotThrow(() => { df.formatRange( "  Infinity   ", "  -Infinity    ")});
assertDoesNotThrow(() => { df.formatRange( "  +Infinity   ", "  -Infinity    ")});
assertDoesNotThrow(() => { df.formatRange( "  Infinity   ", "  -0    ")});
assertDoesNotThrow(() => { df.formatRange( "  +Infinity   ", "  -0    ")});

// other case which won't throw under 3
assertDoesNotThrow(() => { df.formatRange( "  Infinity   ", "  Infinity    ") })
assertEquals("~∞", df.formatRange("     Infinity ", "  Infinity    "));

assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  -1e-30    ")});
assertDoesNotThrow(() => { df.formatRange( "  -0.000e200   ", "  -1e-30    ")});
assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  -Infinity    ")});
// other case which won't throw
assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  Infinity    ") })
assertEquals("-0 – ∞", df.formatRange("     -0 ", "  Infinity    "));
assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  -0    ") })
assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  12345    ") })
assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  12345e-30    ") })
assertEquals("-0 – 0", df.formatRange("     -0 ", "  12345e-30    "));
assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  .12345e-30    ") })
assertDoesNotThrow(() => { df.formatRange( "  -0   ", "  .12345e34    ") })
assertEquals("-0 – 12,345,000,000,000,000,000,000,000,000,000",
    df.formatRange("     -0 ", "  .12345e32    "));

// other cases which won't throw not under 2-4
assertDoesNotThrow(() => { df.formatRange( "  -Infinity   ", "  -Infinity    ") })
assertEquals("~-∞", df.formatRange("     -Infinity ", "  -Infinity    "));
assertDoesNotThrow(() => { df.formatRange( "  -Infinity   ", "  -3e20    ") })
assertDoesNotThrow(() => { df.formatRange( "  -Infinity   ", "  -3e20    ") })
assertDoesNotThrow(() => { df.formatRange( "  -Infinity   ", "  -0    ") })
assertDoesNotThrow(() => { df.formatRange( "  -Infinity   ", "  0    ") })
assertDoesNotThrow(() => { df.formatRange( "  -Infinity   ", "  .3e20    ") })
assertDoesNotThrow(() => { df.formatRange( "  -Infinity   ", "  Infinity    ") })
assertEquals("-∞ – ∞", df.formatRange("     -Infinity ", "  Infinity    "));
