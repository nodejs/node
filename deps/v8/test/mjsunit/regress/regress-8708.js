// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

let array = new Array(1);
array.splice(1, 0, array);

assertThrows(() => array.flat(Infinity), RangeError);
