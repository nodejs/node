// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let foo = 42;
function testFoo(x) { assertEquals(x, foo); }
testFoo(42);
foo++;
testFoo(43);
