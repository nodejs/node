// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let v2 = Object;
const v4 = new Proxy(Object,v2);
const v6 = (9).__proto__;
v6.__proto__ = v4;
function v8(v9,v10,v11) {
    let v14 = 0;
    do {
      const v16 = (0x1337).prototype;
      v14++;
    } while (v14 < 24);
}
const v7 = [1,2,3,4];
const v17 = v7.findIndex(v8);
