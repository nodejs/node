// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test Intl.NumberFormat returns correct minimumIntegerDigits when
// resolvedOptions called
let formatter = new Intl.NumberFormat('en-US', {
    minimumIntegerDigits: 5,
    useGrouping: false,
});
assertEquals("00001", formatter.format(1));
assertEquals("00012", formatter.format(12));
assertEquals("00123", formatter.format(123));
assertEquals("01234", formatter.format(1234));
assertEquals("12345", formatter.format(12345));
assertEquals("123456", formatter.format(123456));
assertEquals("1234567", formatter.format(1234567));
assertEquals(5, formatter.resolvedOptions().minimumIntegerDigits);
