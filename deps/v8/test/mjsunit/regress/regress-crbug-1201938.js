// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let o1 = { a: 1, b: 0 };
let o2 = { a: 2, b: 0 };
o1.__defineGetter__("c", function() { ; return ; });
Object.defineProperty(o2, "b", { value: 4.2, enumerable: true, configurable: true, writable: true, });
let o3 = { a: "foo", b: 0 };
o3.__defineGetter__("c", function() { ; return ; });
Object.defineProperty(o1, "a", { value:2, enumerable: false, configurable: true, writable: true, });
