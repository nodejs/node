// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-bigint --noopt

var a = 5n;
var b = a / -1n;
assertEquals(5n, a);
assertEquals(-5n, b);
assertEquals(5n, 5n / 1n);
assertEquals(5n, -5n / -1n);
assertEquals(-5n, -5n / 1n);

assertEquals(0n, 5n % 1n);
assertEquals(0n, -5n % 1n);
assertEquals(0n, 5n % -1n);
assertEquals(0n, -5n % -1n);
