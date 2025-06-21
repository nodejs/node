// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'calendar'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'dateTimeField'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'language', languageDisplay: 'standard'}));

assertDoesNotThrow(
    () => new Intl.DisplayNames(
        'sr', {type: 'language', languageDisplay: 'dialect'}));
