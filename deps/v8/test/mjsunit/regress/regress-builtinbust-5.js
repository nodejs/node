// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = [ 1, 2, 3 ];
var was_called = false;
function poison() { was_called = true; }
a.hasOwnProperty = poison;
Object.freeze(a);

assertThrows("a.unshift()", TypeError);
assertEquals(3, a.length);
assertFalse(was_called);
