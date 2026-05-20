// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const promiseAllCallOnNonObjectErrorMessage =
    'Promise.all called on non-object';
const promiseAllSettledCallOnNonObjectErrorMessage =
    'Promise.allSettled called on non-object';

assertThrows(
    () => Promise.all.call(), TypeError, promiseAllCallOnNonObjectErrorMessage);
assertThrows(
    () => Promise.allSettled.call(), TypeError,
    promiseAllSettledCallOnNonObjectErrorMessage);
assertThrows(
    () => Promise.all.apply(), TypeError,
    promiseAllCallOnNonObjectErrorMessage);
assertThrows(
    () => Promise.allSettled.apply(), TypeError,
    promiseAllSettledCallOnNonObjectErrorMessage);
