// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-enable-debug-features

// This test ensures that we manage to serialize the global.gaga function for
// compilation and therefore are able to inline it. Since the call feedback in
// bar is megamorphic, this relies on recording the correct accumulator hint for
// the named load of obj.gaga while serializing bar (in turn while serializing
// foo).

const global = this;
global.gaga = function gaga() { return true; };

function bar(obj) { return obj.gaga(); }
function foo(obj) { obj.gaga; %TurbofanStaticAssert(bar(obj)); }

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(global.gaga);

bar({gaga() {}});
foo(global);
%OptimizeFunctionOnNextCall(foo);
foo(global);
