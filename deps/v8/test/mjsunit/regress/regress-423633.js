// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Array.prototype, '0', {
  get: function() { return false; },
});
var a = [1, 2, 3];
assertEquals(a, a.slice());
assertEquals([3], a.splice(2, 1));

a = [1, 2, 3];
a[0xffff] = 4;
// nulling the prototype lets us stay in the sparse case; otherwise the
// getter on Array.prototype would force us into the non-sparse code.
a.__proto__ = null;
assertEquals(a, Array.prototype.slice.call(a));
assertEquals([3], Array.prototype.splice.call(a, 2, 1));
