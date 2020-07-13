// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DisplayNames constructor can't be called as function.
assertThrows(() => Intl.DisplayNames('sr'), TypeError);

assertDoesNotThrow(() => new Intl.DisplayNames('sr', {}));

assertDoesNotThrow(() => new Intl.DisplayNames([], {}));

assertDoesNotThrow(() => new Intl.DisplayNames(['fr', 'ar'], {}));

assertDoesNotThrow(() => new Intl.DisplayNames({0: 'ja', 1:'fr'}, {}));

assertDoesNotThrow(() => new Intl.DisplayNames({1: 'ja', 2:'fr'}, {}));

assertDoesNotThrow(() => new Intl.DisplayNames('sr'));

assertDoesNotThrow(() => new Intl.DisplayNames());

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {
          localeMatcher: 'lookup',
          style: 'short',
          type: 'language',
          fallback: 'code',
        }));


assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {localeMatcher: 'lookup'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {localeMatcher: 'best fit'}));

assertThrows(
    () => new Intl.DisplayNames('sr', {localeMatcher: 'hello'}),
    RangeError);

assertThrows(
    () => new Intl.DisplayNames('sr', {localeMatcher: 'look up'}),
    RangeError);

assertThrows(
    () => new Intl.DisplayNames('sr', {localeMatcher: 'bestfit'}),
    RangeError);


assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {style: 'long'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {style: 'short'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {style: 'narrow'}));

assertThrows(
    () => new Intl.DisplayNames('sr', {style: 'giant'}),
    RangeError);

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {fallback: 'code'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {fallback: 'none'}));

assertThrows(
    () => new Intl.DisplayNames('sr', {fallback: 'never'}),
    RangeError);

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {type: 'language'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {type: 'region'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {type: 'script'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames('sr', {type: 'currency'}));

assertThrows(
    () => new Intl.DisplayNames('sr', {type: ''}),
    RangeError);
