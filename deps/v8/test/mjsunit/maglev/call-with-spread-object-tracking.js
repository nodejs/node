// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-object-tracking

// Spreading arguments object with mutated length (non-inlined & inlined).
(function() {
function target(a, b, c) {
  return '' + a + '|' + b + '|' + c;
}

function non_inlined(a, b, c) {
  'use strict';
  arguments.length = 1;
  return target(...arguments);
}

function mid(a, b, c) {
  'use strict';
  arguments.length = 1;
  return target(...arguments);
}
function inlined(a, b, c) {
  return mid(a, b, c);
}

%PrepareFunctionForOptimization(target);
%PrepareFunctionForOptimization(mid);
for (let f of [non_inlined, inlined]) {
  %PrepareFunctionForOptimization(f);
  assertEquals('1|undefined|undefined', f(1, 2, 3));
  assertEquals('1|undefined|undefined', f(1, 2, 3));
  %OptimizeMaglevOnNextCall(f);
  assertEquals('1|undefined|undefined', f(1, 2, 3));
  assertOptimized(f);
}
})();
