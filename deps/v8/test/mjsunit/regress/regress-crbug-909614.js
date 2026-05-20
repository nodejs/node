// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let just_under = 2n ** 30n - 1n;
let just_above = 2n ** 30n;

assertDoesNotThrow(() => { var dummy = 2n ** just_under; });
assertThrows(() => { var dummy = 2n ** just_above; });
