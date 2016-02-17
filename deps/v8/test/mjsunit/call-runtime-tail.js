// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --nostress-opt --turbo
// Flags: --nonative-context-specialization

var p0 = new Object();
var p1 = new Object();
var p2 = new Object();

// Ensure 1 parameter passed straight-through is handled correctly
var count1 = 100000;
tailee1 = function() {
  "use strict";
  if (count1-- == 0) {
    return this;
  }
  return %_Call(tailee1, this);
};

%OptimizeFunctionOnNextCall(tailee1);
assertEquals(p0, tailee1.call(p0));

// Ensure 2 parameters passed straight-through trigger a tail call are handled
// correctly and don't cause a stack overflow.
var count2 = 100000;
tailee2 = function(px) {
  "use strict";
  assertEquals(p2, px);
  assertEquals(p1, this);
  count2 = ((count2 | 0) - 1) | 0;
  if ((count2 | 0) === 0) {
    return this;
  }
  return %_Call(tailee2, this, px);
};

%OptimizeFunctionOnNextCall(tailee2);
assertEquals(p1, tailee2.call(p1, p2));

// Ensure swapped 2 parameters don't trigger a tail call (parameter swizzling
// for the tail call isn't supported yet).
var count3 = 100000;
tailee3 = function(px) {
  "use strict";
  if (count3-- == 0) {
    return this;
  }
  return %_Call(tailee3, px, this);
};

%OptimizeFunctionOnNextCall(tailee3);
assertThrows(function() { tailee3.call(p1, p2); });

// Ensure too many parameters defeats the tail call optimization (currently
// unsupported).
var count4 = 1000000;
tailee4 = function(px) {
  "use strict";
  if (count4-- == 0) {
    return this;
  }
  return %_Call(tailee4, this, px, undefined);
};

%OptimizeFunctionOnNextCall(tailee4);
assertThrows(function() { tailee4.call(p1, p2); });

// Ensure too few parameters defeats the tail call optimization (currently
// unsupported).
var count5 = 1000000;
tailee5 = function(px) {
  "use strict";
  if (count5-- == 0) {
    return this;
  }
  return %_Call(tailee5, this);
};

%OptimizeFunctionOnNextCall(tailee5);
assertThrows(function() { tailee5.call(p1, p2); });
