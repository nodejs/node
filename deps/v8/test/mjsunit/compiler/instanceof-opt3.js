// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Bar() {}
var Foo = Bar.bind(null);

// TurboFan will optimize this to false via constant-folding the
// OrdinaryHasInstance call inside Function.prototype[@@hasInstance].
function foo() { return 1 instanceof Foo; }

%PrepareFunctionForOptimization(foo);
assertEquals(false, foo());
assertEquals(false, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(false, foo());
