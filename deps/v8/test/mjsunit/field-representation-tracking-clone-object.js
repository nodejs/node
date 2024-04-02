// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(o) { return {...o}; }

// Warmup the CloneObjectIC with `o.x` being
// a HeapObject field.
var o = {data:null};
%PrepareFunctionForOptimization(foo);
foo(o);
foo(o);

// Now update the field representation of o.x
// in-place from HeapObject to Tagged and make
// sure that this is handled properly in the
// fast-path for CloneObjectIC.
o.data = 1;
assertEquals(1, %GetProperty(foo(o), "data"));
