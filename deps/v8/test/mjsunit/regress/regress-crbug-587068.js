// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// The Crankshaft fast case for String.fromCharCode used to unconditionally
// deoptimize on non int32 indices.
function foo(i) { return String.fromCharCode(i); }
foo(33);
foo(33);
%OptimizeFunctionOnNextCall(foo);
foo(33.3);
assertOptimized(foo);
