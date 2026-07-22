// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var result1, result2;
import('./modules-skip-regress-crbug-424617296.js').then(
    () => assertUnreachable('Should have failed due to non-existing module'),
    error => result1 = error.message);
import('./modules-skip-regress-crbug-424617296.js').then(
    () => assertUnreachable('Should have failed due to non-existing module'),
    error => result2 = error.message);

%PerformMicrotaskCheckpoint();

assertEquals(result1, result2);
assertMatches('d8: Error reading module from .*/this-module-does-not-exist.js',
    result1);
