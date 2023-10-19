// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/1223724
const kBits = 500 * 64;
const a = BigInt.asUintN(kBits, -1n);
const b = a * a;
const expected = (a << BigInt(kBits)) - a;
assertEquals(expected, b);
