// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --allow-natives-syntax --stress-compaction

function foo() { return "foo"; }

%PrepareFunctionForOptimization(foo);
let value = foo();
assertTrue(%IsSharedString(value));
%OptimizeFunctionOnNextCall(foo);
value = foo();
assertTrue(%IsSharedString(value));
%SharedGC();
value = foo();
assertTrue(%IsSharedString(value));
assertEquals("foo", value);
