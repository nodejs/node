// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function boom(a1, a2) {
  // Do something with a2 that needs a map check (for DOUBLE_ELEMENTS).
  var s = a2[0];
  // Emit a load that transitions a1 to PACKED_ELEMENTS.
  var t = a1[0];
  // Emit a store to a2 that assumes DOUBLE_ELEMENTS.
  // The map check is considered redundant and will be eliminated.
  a2[0] = 0.3;
}

// Prepare type feedback for the "t = a1[0]" load: fast elements.
;
%PrepareFunctionForOptimization(boom);
var fast_elem = new Array(1);
fast_elem[0] = 'tagged';
boom(fast_elem, [1]);

// Prepare type feedback for the "a2[0] = 0.3" store: double elements.
var double_elem = new Array(1);
double_elem[0] = 0.1;
boom(double_elem, double_elem);

// Reset |double_elem| and go have a party.
double_elem = new Array(10);
double_elem[0] = 0.1;

%OptimizeFunctionOnNextCall(boom);
boom(double_elem, double_elem);

assertEquals(0.3, double_elem[0]);
assertEquals(undefined, double_elem[1]);
