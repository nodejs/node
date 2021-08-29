// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let quotient = 0x1234567890abcdef12345678n;  // Bigger than one digit_t.
let dividend = quotient * 10n;
let result = dividend % quotient;
assertEquals(0n, result);
