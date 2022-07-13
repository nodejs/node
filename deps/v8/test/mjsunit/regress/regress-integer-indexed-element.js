// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = {__proto__:new Int32Array(100)};
Object.prototype[1.3] = 10;
assertEquals(undefined, o[1.3]);

var o = new Int32Array(100);
var o2 = new Int32Array(200);
o.__proto__ = o2;
assertEquals(undefined, Reflect.get(o, 1.3, o2));
