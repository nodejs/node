// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(BigInt("-0 ") == -0);
assertTrue("-0 " == 0n);
assertTrue(BigInt("-0") == -0);
assertTrue(-0n == -0);
assertTrue(-0n == 0n);

assertTrue(BigInt("-0 ") > -1);
assertTrue("-0 " > -1n);
assertTrue(BigInt("-0") > -1);
assertTrue(-0n > -1);

assertEquals(BigInt("-0 ") & 1n, 0n);
assertEquals(BigInt("-0") & 1n, 0n);
assertEquals(-0n & 1n, 0n);

var zero = BigInt("-0 ");
assertEquals(1n, ++zero);
zero = BigInt("-0");
assertEquals(1n, ++zero);
zero = -0n;
assertEquals(1n, ++zero);
