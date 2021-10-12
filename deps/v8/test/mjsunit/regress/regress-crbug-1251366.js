// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Object.prototype, "x", {
    set: function (val) {}
});

let o = {...{}, x: 3};
assertEquals(o.x, 3);

let o2 = {...{x: 1}, x: 3};
assertEquals(o2.x, 3);
