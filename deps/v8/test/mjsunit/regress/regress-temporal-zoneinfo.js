// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for the resb parser originally only supporting little
// endian ICU resource bundles, causing Temporal timezone operations to
// fail on big endian platforms. More details: http://crrev.com/c/7583895

const instant = Temporal.Instant.from('1969-07-20T20:17:00Z');
const zdt = instant.toZonedDateTimeISO('America/New_York');
assertEquals(zdt.offset, "-04:00");
