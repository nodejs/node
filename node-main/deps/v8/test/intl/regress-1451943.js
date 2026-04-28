// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let beforeOct1582GregorianTransition = new Date('1582-01-01T00:00Z');
let afterOct1582GregorianTransition = new Date('1583-01-01T00:00Z');

// Gregorian (proleptic) calendar
assertEquals(
    "1/1/1582",
    beforeOct1582GregorianTransition.toLocaleDateString('en-US', { timeZone: 'UTC', calendar: 'gregory' }));
// ISO 8601 calendar output before Oct 1582 Gregorian transition
assertEquals(
    "1/1/1582",
    beforeOct1582GregorianTransition.toLocaleDateString('en-US', { timeZone: 'UTC', calendar: 'iso8601' }));
// ISO 8601 calendar output after the Gregorian transition
assertEquals(
    "1/1/1583",
    afterOct1582GregorianTransition.toLocaleDateString('en-US', { timeZone: 'UTC', calendar: 'iso8601' }));
