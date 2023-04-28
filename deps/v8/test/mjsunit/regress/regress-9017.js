// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Call a recursive function that uses large numbers of bound arguments. If we
// are failing to touch consecutive guard pages on Windows when extending the
// stack for bound arguments, then this would crash.

const frameSize = 4096 * 5;
const numValues = frameSize / 4;
const arr = new Array(numValues);
let counter = 10;
function f() { --counter; return 1 + (counter > 0 ? bound() : 0); }
const bound = f.bind.apply(f, arr);
bound();
