// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test eager deopt when the object is not a JSGeneratorObject
function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

function* myGen() {}
let generatorObject = myGen();
test(generatorObject);

let fake = { next: generatorObject.next };

%OptimizeMaglevOnNextCall(test);
assertThrows(() => test(fake), TypeError);
assertUnoptimized(test);
