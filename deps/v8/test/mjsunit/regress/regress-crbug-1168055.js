// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let arr1 = [{
    set ['a'](x) {
        super.x;
    },
    y: 1
}];
let arr2 = new Float32Array(arr1);
