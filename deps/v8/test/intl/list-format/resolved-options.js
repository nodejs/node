// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-list-format

let listFormat = new Intl.ListFormat();
// The default style is 'long'
assertEquals('long', listFormat.resolvedOptions().style);

// The default type is 'conjunction'
assertEquals('conjunction', listFormat.resolvedOptions().type);

assertEquals(
    'short',
    (new Intl.ListFormat(['sr'], {style: 'short'}))
        .resolvedOptions().style);

assertEquals(
    'conjunction',
    (new Intl.ListFormat(['sr'], {style: 'short'}))
        .resolvedOptions().type);

assertEquals(
    'long',
    (new Intl.ListFormat(['sr'], {style: 'long'}))
        .resolvedOptions().style);

assertEquals(
    'conjunction',
    (new Intl.ListFormat(['sr'], {style: 'long'}))
        .resolvedOptions().type);

assertEquals(
    'conjunction',
    (new Intl.ListFormat(['sr'], {type: 'conjunction'}))
        .resolvedOptions().type);

assertEquals(
    'long',
    (new Intl.ListFormat(['sr'], {type: 'conjunction'}))
        .resolvedOptions().style);

assertEquals(
    'disjunction',
    (new Intl.ListFormat(['sr'], {type: 'disjunction'}))
        .resolvedOptions().type);

assertEquals(
    'long',
    (new Intl.ListFormat(['sr'], {type: 'disjunction'}))
        .resolvedOptions().style);

assertEquals(
    'unit',
    (new Intl.ListFormat(['sr'], {type: 'unit'}))
        .resolvedOptions().type);

assertEquals(
    'long',
    (new Intl.ListFormat(['sr'], {type: 'unit'}))
        .resolvedOptions().style);

assertEquals(
    'conjunction',
    (new Intl.ListFormat(['sr'], {style: 'long', type: 'conjunction'}))
        .resolvedOptions().type);

assertEquals(
    'long',
    (new Intl.ListFormat(['sr'], {style: 'long', type: 'conjunction'}))
        .resolvedOptions().style);

assertEquals(
    'conjunction',
    (new Intl.ListFormat(['sr'], {style: 'short', type: 'conjunction'}))
        .resolvedOptions().type);

assertEquals(
    'short',
    (new Intl.ListFormat(['sr'], {style: 'short', type: 'conjunction'}))
        .resolvedOptions().style);

assertEquals(
    'disjunction',
    (new Intl.ListFormat(['sr'], {style: 'long', type: 'disjunction'}))
        .resolvedOptions().type);

assertEquals(
    'long',
    (new Intl.ListFormat(['sr'], {style: 'long', type: 'disjunction'}))
        .resolvedOptions().style);

assertEquals(
    'disjunction',
    (new Intl.ListFormat(['sr'], {style: 'short', type: 'disjunction'}))
        .resolvedOptions().type);

assertEquals(
    'short',
    (new Intl.ListFormat(['sr'], {style: 'short', type: 'disjunction'}))
        .resolvedOptions().style);

assertEquals(
    'unit',
    (new Intl.ListFormat(['sr'], {style: 'long', type: 'unit'}))
        .resolvedOptions().type);

assertEquals(
    'long',
    (new Intl.ListFormat(['sr'], {style: 'long', type: 'unit'}))
        .resolvedOptions().style);

assertEquals(
    'unit',
    (new Intl.ListFormat(['sr'], {style: 'short', type: 'unit'}))
        .resolvedOptions().type);

assertEquals(
    'short',
    (new Intl.ListFormat(['sr'], {style: 'short', type: 'unit'}))
        .resolvedOptions().style);

assertEquals(
    'unit',
    (new Intl.ListFormat(['sr'], {style: 'narrow', type: 'unit'}))
        .resolvedOptions().type);

assertEquals(
    'narrow',
    (new Intl.ListFormat(['sr'], {style: 'narrow', type: 'unit'}))
        .resolvedOptions().style);

assertEquals(
    'ar',
    (new Intl.ListFormat(['ar'])).resolvedOptions().locale);

assertEquals(
    'ar',
    (new Intl.ListFormat(['ar', 'en'])).resolvedOptions().locale);

assertEquals(
    'fr',
    (new Intl.ListFormat(['fr', 'en'])).resolvedOptions().locale);

assertEquals(
    'ar',
    (new Intl.ListFormat(['xyz', 'ar'])).resolvedOptions().locale);

assertEquals(
    'ar',
    (new Intl.ListFormat(['i-default', 'ar'])).resolvedOptions().locale);
