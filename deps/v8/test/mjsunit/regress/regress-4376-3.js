// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Foo() {}
var x = new Foo();
function foo() { return x instanceof Foo; }
assertTrue(foo());
Foo.prototype = 1;
assertThrows(foo, TypeError);
