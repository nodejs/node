// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo($a,$b) {
 $a = $a|0;
 $b = $b|0;
 var $sub = $a - $b;
 return ($sub|0) < 0;
}

%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(0x7fffffff,-1));
