// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noenable-slow-asserts

// This call ensures that TurboFan won't inline array constructors.
Array(2**30);

// Set up a fast holey smi array, and generate optimized code.
let a = [1, 2, ,,, 3];
function mapping(a) {
  return a.map(v => v);
}
mapping(a);
mapping(a);
%OptimizeFunctionOnNextCall(mapping);
mapping(a);

// Now lengthen the array, but ensure that it points to a non-dictionary
// backing store.
a.length = (32 * 1024 * 1024)-1;
a.fill(1,0);
a.push(2);
a.length += 500;
// Now, the non-inlined array constructor should produce an array with
// dictionary elements: causing a crash.
mapping(a);
