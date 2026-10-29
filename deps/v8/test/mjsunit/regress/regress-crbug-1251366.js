// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Object.prototype, "x", {
    set: function (val) {}
});

Object.defineProperty(Object.prototype, "0", {
    set: function (val) {}
});

let o = {...{}, x: 3};
assertEquals(3, o.x);

let o2 = {...{x: 1}, x: 3};
assertEquals(3, o2.x);

let o3 = {...{}, 0: 42}
assertEquals(42, o3[0]);
