// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for https://issues.chromium.org/issues/488366773
// Array.prototype.flat must not crash on HOLEY_DOUBLE_ELEMENTS arrays
// that contain undefined (V8_ENABLE_UNDEFINED_DOUBLE).

var a = [];
Object.defineProperty(a, '1', { get: function() {} });
var b = a.slice();
var result = b.flat();
assertEquals(1, result.length);
assertEquals(undefined, result[0]);
