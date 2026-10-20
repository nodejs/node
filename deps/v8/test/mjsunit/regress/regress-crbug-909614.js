// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let kDigitBits = %Is64Bit() ? 64n : 32n;

let just_under = 2n ** 30n - kDigitBits - 1n;
let just_above = 2n ** 30n - kDigitBits;

assertDoesNotThrow(() => { var dummy = 2n ** just_under; });
assertThrows(() => { var dummy = 2n ** just_above; });
