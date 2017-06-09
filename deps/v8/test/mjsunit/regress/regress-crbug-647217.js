// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stack-size=100 --ignition --turbo

var source = "return 1" + new Array(2048).join(' + a') + "";
eval("function g(a) {" + source + "}");
%SetForceInlineFlag(g);

function f(a) { return g(a) }
%OptimizeFunctionOnNextCall(f);
try { f(0) } catch(e) {}
