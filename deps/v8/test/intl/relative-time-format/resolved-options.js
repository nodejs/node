// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let rtf = new Intl.RelativeTimeFormat();
// Test 1.4.5 Intl.RelativeTimeFormat.prototype.resolvedOptions ()
// The default style is 'long'
assertEquals('long', rtf.resolvedOptions().style);

// The default numeric is 'always'
assertEquals('always', rtf.resolvedOptions().numeric);

// contains style, numeric and locale key
assertEquals(4, Object.getOwnPropertyNames(rtf.resolvedOptions()).length);

// contains style, numeric and locale key
assertEquals(
    4,
    Object.getOwnPropertyNames(
        new Intl.RelativeTimeFormat("en").resolvedOptions()
    ).length
);

assertEquals(
    'short',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'short'}))
        .resolvedOptions().style);

assertEquals(
    'always',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'short'}))
        .resolvedOptions().numeric);

assertEquals(
    'narrow',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'narrow'}))
        .resolvedOptions().style);

assertEquals(
    'always',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'narrow'}))
        .resolvedOptions().numeric);

assertEquals(
    'long',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'long'}))
        .resolvedOptions().style);

assertEquals(
    'always',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'long'}))
        .resolvedOptions().numeric);

assertEquals(
    'auto',
    (new Intl.RelativeTimeFormat(['sr'], {numeric: 'auto'}))
        .resolvedOptions().numeric);

assertEquals(
    'long',
    (new Intl.RelativeTimeFormat(['sr'], {numeric: 'auto'}))
        .resolvedOptions().style);

assertEquals(
    'always',
    (new Intl.RelativeTimeFormat(['sr'], {numeric: 'always'}))
        .resolvedOptions().numeric);

assertEquals(
    'long',
    (new Intl.RelativeTimeFormat(['sr'], {numeric: 'always'}))
        .resolvedOptions().style);

assertEquals(
    'long',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'long', numeric: 'auto'}))
        .resolvedOptions().style);

assertEquals(
    'auto',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'long', numeric: 'auto'}))
        .resolvedOptions().numeric);

assertEquals(
    'long',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'long', numeric: 'always'}))
        .resolvedOptions().style);

assertEquals(
    'always',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'long', numeric: 'always'}))
        .resolvedOptions().numeric);

assertEquals(
    'short',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'short', numeric: 'auto'}))
        .resolvedOptions().style);

assertEquals(
    'auto',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'short', numeric: 'auto'}))
        .resolvedOptions().numeric);

assertEquals(
    'short',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'short', numeric: 'always'}))
        .resolvedOptions().style);

assertEquals(
    'always',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'short', numeric: 'always'}))
        .resolvedOptions().numeric);

assertEquals(
    'narrow',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'narrow', numeric: 'auto'}))
        .resolvedOptions().style);

assertEquals(
    'auto',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'narrow', numeric: 'auto'}))
        .resolvedOptions().numeric);

assertEquals(
    'narrow',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'narrow', numeric: 'always'}))
        .resolvedOptions().style);

assertEquals(
    'always',
    (new Intl.RelativeTimeFormat(['sr'], {style: 'narrow', numeric: 'always'}))
        .resolvedOptions().numeric);

assertEquals(
    'ar',
    (new Intl.RelativeTimeFormat(['ar'])).resolvedOptions().locale);

assertEquals(
    'ar',
    (new Intl.RelativeTimeFormat(['ar', 'en'])).resolvedOptions().locale);

assertEquals(
    'fr',
    (new Intl.RelativeTimeFormat(['fr', 'en'])).resolvedOptions().locale);

assertEquals(
    'ar',
    (new Intl.RelativeTimeFormat(['xyz', 'ar'])).resolvedOptions().locale);

{
  var receiver = 1;
  assertThrows(() =>
     Intl.RelativeTimeFormat.prototype.resolvedOptions.call(receiver), TypeError);

  receiver = {};
  assertThrows(() =>
     Intl.RelativeTimeFormat.prototype.resolvedOptions.call(receiver), TypeError);
}

assertEquals(
    'ar',
    (new Intl.RelativeTimeFormat(['i-default', 'ar'])).resolvedOptions().locale);
