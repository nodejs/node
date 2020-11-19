// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(
    () => (new Intl.DateTimeFormat("en", {fractionalSecondDigits: 0})),
    RangeError,
    "fractionalSecondDigits value is out of range.");

assertThrows(
    () => (new Intl.DateTimeFormat("en", {fractionalSecondDigits: 4})),
    RangeError,
    "fractionalSecondDigits value is out of range.");

assertEquals(
    1,
    (new Intl.DateTimeFormat("en", {fractionalSecondDigits: 1}))
        .resolvedOptions().fractionalSecondDigits);

assertEquals(
    2,
    (new Intl.DateTimeFormat("en", {fractionalSecondDigits: 2}))
        .resolvedOptions().fractionalSecondDigits);

assertEquals(
    3,
    (new Intl.DateTimeFormat("en", {fractionalSecondDigits: 3}))
        .resolvedOptions().fractionalSecondDigits);

// When timeStyle and dateStyle is not present, GetNumberOption will fallback
// to undefined as default regardless fractionalSecondDigits is present in the option or
// not.
assertEquals(
    undefined,
    (new Intl.DateTimeFormat()).resolvedOptions().fractionalSecondDigits);

assertEquals(
    undefined,
    (new Intl.DateTimeFormat("en", {fractionalSecondDigits: undefined}))
        .resolvedOptions().fractionalSecondDigits);

// When timeStyle or dateStyle is present, we should throw TypeError
assertThrows(
    () => (new Intl.DateTimeFormat(
        "en", {timeStyle: "short", fractionalSecondDigits: 3})),
    TypeError,
    "Can't set option fractionalSecondDigits when timeStyle is used");

assertThrows(
    () => (new Intl.DateTimeFormat(
        "en", {dateStyle: "short", fractionalSecondDigits: 3})),
    TypeError,
    "Can't set option fractionalSecondDigits when dateStyle is used");
