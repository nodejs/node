// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj = {};
let arr = new Uint8Array(3);
function __f_0() {
        arr[2] = obj;
}
obj.toString = __f_0;
assertThrows(() => obj.toString(), RangeError);
