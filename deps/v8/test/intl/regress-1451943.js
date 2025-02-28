// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let beforeOct1582GregorianTransition = new Date('1582-01-01T00:00Z');
let afterOct1582GregorianTransition = new Date('1583-01-01T00:00Z');

// Gregorian (proleptic) calendar
assertEquals(
    beforeOct1582GregorianTransition.toLocaleDateString('en-US', { timeZone: 'UTC', calendar: 'gregory' }),
    "1/1/1582");
// ISO 8601 calendar output before Oct 1582 Gregorian transition
assertEquals(
    beforeOct1582GregorianTransition.toLocaleDateString('en-US', { timeZone: 'UTC', calendar: 'iso8601' }),
    "1/1/1582");
// ISO 8601 calendar output after the Gregorian transition
assertEquals(
    afterOct1582GregorianTransition.toLocaleDateString('en-US', { timeZone: 'UTC', calendar: 'iso8601' }),
    "1/1/1583");
