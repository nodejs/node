// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

// Not guaranteed to throw because the stack limits are different on all
// architectures, hence we use try-catch instead of assertThrows here.
try { f() } catch(e) { assertInstanceof(e, RangeError) }

function f() {
  return Math.max(
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" + "a" +
    "boom", 1, 2, 3, 4, 5, 6, 7, 8, 9);
};
