// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/1232733
let big = 10n ** 900n;
let expected = "1" + "0".repeat(900);
assertEquals(expected, big.toString());
