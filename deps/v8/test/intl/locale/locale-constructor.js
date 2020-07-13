// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Locale constructor can't be called as function.
assertThrows(() => Intl.Locale('sr'), TypeError);

// Non-string locale.
assertThrows(() => new Intl.Locale(5), TypeError);
assertThrows(() => new Intl.Locale(Symbol()), TypeError);
assertThrows(() => new Intl.Locale(null), TypeError);
assertThrows(() => new Intl.Locale(undefined), TypeError);
assertThrows(() => new Intl.Locale(false), TypeError);
assertThrows(() => new Intl.Locale(true), TypeError);

// Invalid locale string.
assertThrows(() => new Intl.Locale('abcdefghi'), RangeError);

// Options will be force converted into Object.
assertDoesNotThrow(() => new Intl.Locale('sr', 5));

// Regression for http://bugs.icu-project.org/trac/ticket/13417.
assertDoesNotThrow(
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
    RangeError);

// Throws only once during construction.
// Check for all getters to prevent regression.
assertThrows(
    () => new Intl.Locale('en-US', {
      get calendar() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get caseFirst() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get collation() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get hourCycle() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get numeric() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get numberingSystem() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get language() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get script() {
        throw new Error('foo');
      }
    }),
    Error);

assertThrows(
    () => new Intl.Locale('en-US', {
      get region() {
        throw new Error('foo');
      }
    }),
    Error);

// There won't be an override for baseName so we don't expect it to throw.
assertDoesNotThrow(
    () => new Intl.Locale('en-US', {
      get baseName() {
        throw new Error('foo');
      }
    }),
    Error);

// Preserve the order of getter initialization.
let getCount = 0;
let calendar = -1;
let collation = -1;
let hourCycle = -1;
let caseFirst = -1;
let numeric = -1;
let numberingSystem = -1;


new Intl.Locale('en-US', {
  get calendar() {
    calendar = ++getCount;
  },
  get collation() {
    collation = ++getCount;
  },
  get hourCycle() {
    hourCycle = ++getCount;
  },
  get caseFirst() {
    caseFirst = ++getCount;
  },
  get numeric() {
    numeric = ++getCount;
  },
  get numberingSystem() {
    numberingSystem = ++getCount;
  },
});

assertEquals(1, calendar);
assertEquals(2, collation);
assertEquals(3, hourCycle);
assertEquals(4, caseFirst);
assertEquals(5, numeric);
assertEquals(6, numberingSystem);

// Check getter properties against the spec.
function checkProperties(property) {
  let desc = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, property);
  assertEquals(`get ${property}`, desc.get.name);
  assertEquals('function', typeof desc.get)
  assertEquals(undefined, desc.set);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
}

checkProperties('language');
checkProperties('script');
checkProperties('region');
checkProperties('baseName');
checkProperties('calendar');
checkProperties('collation');
checkProperties('hourCycle');
checkProperties('caseFirst');
checkProperties('numeric');
checkProperties('numberingSystem');
