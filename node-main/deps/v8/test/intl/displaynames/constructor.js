// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DisplayNames constructor can't be called as function.
assertThrows(() => Intl.DisplayNames('sr'), TypeError);
assertThrows(() => Intl.DisplayNames('sr', {}), TypeError);
assertThrows(() => Intl.DisplayNames(undefined, {}), TypeError);

assertDoesNotThrow(() =>
    new Intl.DisplayNames('sr', {type: 'language'}));

assertDoesNotThrow(() =>
    new Intl.DisplayNames([], {type: 'language'}));

assertDoesNotThrow(() =>
    new Intl.DisplayNames(['fr', 'ar'], {type: 'language'}));

assertDoesNotThrow(() =>
    new Intl.DisplayNames({0: 'ja', 1:'fr'}, {type: 'language'}));

assertDoesNotThrow(() =>
    new Intl.DisplayNames({1: 'ja', 2:'fr'}, {type: 'language'}));

assertDoesNotThrow(() =>
    new Intl.DisplayNames('sr', {type: 'language'}));

assertDoesNotThrow(() =>
    new Intl.DisplayNames(undefined, {type: 'language'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {
          localeMatcher: 'lookup',
          style: 'short',
          type: 'language',
          fallback: 'code',
        }));


assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {localeMatcher: 'lookup', type: 'language'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {localeMatcher: 'best fit', type: 'language'}));

assertThrows(
    () => new Intl.DisplayNames(
        'sr', {localeMatcher: 'hello', type: 'language'}),
    RangeError);

assertThrows(
    () => new Intl.DisplayNames(
        'sr', {localeMatcher: 'look up', type: 'language'}),
    RangeError);

assertThrows(
    () => new Intl.DisplayNames(
        'sr', {localeMatcher: 'bestfit', type: 'language'}),
    RangeError);


assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {style: 'long', type: 'language'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {style: 'short', type: 'language'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {style: 'narrow', type: 'language'}));

assertThrows(
    () => new Intl.DisplayNames(
        'sr', {style: 'giant', type: 'language'}),
    RangeError);

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {fallback: 'code', type: 'language'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {fallback: 'none', type: 'language'}));

assertThrows(
    () => new Intl.DisplayNames(
        'sr', {fallback: 'never', type: 'language'}),
    RangeError);

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'language'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'region'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'script'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'currency'}));

assertThrows(
    () => new Intl.DisplayNames(
        'sr', {type: ''}),
    RangeError);
