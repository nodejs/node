// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-locale

// Locale constructor can't be called as function.
assertThrows(() => Intl.Locale('sr'), TypeError);

// Non-string locale.
assertThrows(() => new Intl.Locale(5), TypeError);

// Invalid locale.
assertThrows(() => new Intl.Locale('abcdefghi'), TypeError);

// Options will be force converted into Object.
assertDoesNotThrow(() => new Intl.Locale('sr', 5));

// ICU problem - locale length is limited.
// http://bugs.icu-project.org/trac/ticket/13417.
assertThrows(
    () => new Intl.Locale(
        'sr-cyrl-rs-t-ja-u-ca-islamic-cu-rsd-tz-uslax-x-whatever', {
          calendar: 'buddhist',
          caseFirst: 'true',
          collation: 'phonebk',
          hourCycle: 'h23',
          caseFirst: 'upper',
          numeric: 'true',
          numberingSystem: 'roman',
        }),
    TypeError);
