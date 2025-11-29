// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Bar() { }
function Baz() { }
Baz.prototype = { __proto__: new Bar() }
var x = new Baz();
function foo(y) { return y instanceof Bar; }
assertTrue(foo(x));
Baz.prototype.__proto__ = null;
assertFalse(foo(x));
