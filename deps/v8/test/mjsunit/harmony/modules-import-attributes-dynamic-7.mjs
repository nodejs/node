// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-import-attributes

var result1;
var result2;
var result3;
var result4;
var result5;
var result6;
var result7;
var result8;
var result9;
var result10;
import('modules-skip-1.json', null).then(
    () => assertUnreachable('Should have failed due to non-object parameter'),
    error => result1 = error.message);
import('modules-skip-1.json', 7).then(
    () => assertUnreachable('Should have failed due to non-object parameter'),
    error => result2 = error.message);
import('modules-skip-1.json', 'string').then(
        () => assertUnreachable('Should have failed due to non-object parameter'),
        error => result3 = error.message);
import('modules-skip-1.json', { with: null}).then(
    () => assertUnreachable('Should have failed due to bad with object'),
    error => result4 = error.message);
import('modules-skip-1.json', { with: 7}).then(
    () => assertUnreachable('Should have failed due to bad with object'),
    error => result5 = error.message);
import('modules-skip-1.json', { with: 'string'}).then(
    () => assertUnreachable('Should have failed due to bad with object'),
    error => result6 = error.message);
import('modules-skip-1.json', { with: { a: null }}).then(
    () => assertUnreachable('Should have failed due to bad with object'),
    error => result7 = error.message);
import('modules-skip-1.json', { with: { a: undefined }}).then(
    () => assertUnreachable('Should have failed due to bad assertion value'),
    error => result8 = error.message);
import('modules-skip-1.json', { with: { a: 7 }}).then(
    () => assertUnreachable('Should have failed due to bad assertion value'),
    error => result9 = error.message);
    import('modules-skip-1.json', { with: { a: { } }}).then(
        () => assertUnreachable('Should have failed due to bad assertion value'),
        error => result10 = error.message);

%PerformMicrotaskCheckpoint();

const argumentNotObjectError = 'The second argument to import() must be an object';
const assertOptionNotObjectError = 'The \'assert\' option must be an object';
const assertionValueNotStringError = 'Import assertion value must be a string';

assertEquals(argumentNotObjectError, result1);
assertEquals(argumentNotObjectError, result2);
assertEquals(argumentNotObjectError, result3);
assertEquals(assertOptionNotObjectError, result4);
assertEquals(assertOptionNotObjectError, result5);
assertEquals(assertOptionNotObjectError, result6);
assertEquals(assertionValueNotStringError, result7);
assertEquals(assertionValueNotStringError, result8);
assertEquals(assertionValueNotStringError, result9);
assertEquals(assertionValueNotStringError, result10);
