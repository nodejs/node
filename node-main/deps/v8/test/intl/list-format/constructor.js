// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ListFormat constructor can't be called as function.
assertThrows(() => Intl.ListFormat(['sr']), TypeError);

// Non-string locale.
// assertThrows(() => new Intl.ListFormat(5), TypeError);

// Invalid locale string.
assertThrows(() => new Intl.ListFormat(['abcdefghi']), RangeError);

assertDoesNotThrow(() => new Intl.ListFormat(['sr'], {}), TypeError);

assertDoesNotThrow(() => new Intl.ListFormat([], {}));

assertDoesNotThrow(() => new Intl.ListFormat(['fr', 'ar'], {}));

assertDoesNotThrow(() => new Intl.ListFormat({0: 'ja', 1:'fr'}, {}));

assertDoesNotThrow(() => new Intl.ListFormat({1: 'ja', 2:'fr'}, {}));

assertDoesNotThrow(() => new Intl.ListFormat(['sr']));

assertDoesNotThrow(() => new Intl.ListFormat());

assertDoesNotThrow(
    () => new Intl.ListFormat(
        ['sr'], {
          style: 'short',
          type: 'unit'
        }));


assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'conjunction'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'disjunction'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'unit'}));

assertThrows(
    () => new Intl.ListFormat(['sr'], {type: 'standard'}),
    RangeError);

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {style: 'long'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {style: 'short'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {style: 'narrow'}));

assertThrows(
    () => new Intl.ListFormat(['sr'], {style: 'giant'}),
    RangeError);

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'conjunction', style: 'long'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'conjunction', style: 'short'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'conjunction', style: 'narrow'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'disjunction', style: 'long'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'disjunction', style: 'short'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'disjunction', style: 'narrow'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'unit', style: 'long'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'unit', style: 'short'}));

assertDoesNotThrow(
    () => new Intl.ListFormat(['sr'], {type: 'unit', style: 'narrow'}));
