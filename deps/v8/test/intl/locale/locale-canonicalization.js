// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-locale

// Make sure that locale string got canonicalized by the spec,
// keys are sorted and unique, region upper cased, script title cased and
// language lower cased.

let locale = new Intl.Locale('sr-cyrl-rs-t-ja-u-ca-islamic-x-whatever', {
  calendar: 'buddhist',
  caseFirst: 'true',
  collation: 'phonebk',
  hourCycle: 'h23',
  caseFirst: 'upper',
  numeric: 'true',
  numberingSystem: 'roman'
});

let expected =
    'sr-Cyrl-RS-t-ja-u-ca-buddhist-co-phonebk-hc-h23-kf-upper-kn-true-nu-roman-x-whatever';

assertEquals(expected, locale.toString());
