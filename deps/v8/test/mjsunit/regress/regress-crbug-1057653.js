// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.prototype.length = 3642395160;
const array = new Float32Array(2**28);

assertThrows(() => {for (const key in array) {}}, RangeError);
