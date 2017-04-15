// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {}
foo.prototype = 1;
v = new foo();
function bar() { return v instanceof foo; }
assertThrows(bar);
