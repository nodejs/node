// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(a) { a[0](true); }
function foo(a) { return bar(1); }
%OptimizeFunctionOnNextCall(foo);
assertThrows(function() {bar([foo])}, TypeError);
