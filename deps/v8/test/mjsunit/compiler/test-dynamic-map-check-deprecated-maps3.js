// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --turbo-dynamic-map-checks
// Flags: --opt --no-always-opt --deopt-every-n-times=0

function b(a) { return a; }

function f(o, should_bailout) {
  b(o.a);
  let did_bailout = (%GetOptimizationStatus(f) &
                    V8OptimizationStatus.kTopmostFrameIsTurboFanned) == 0;
  assertEquals(should_bailout, did_bailout);
}

var o = {a:10, b:20, c:30};
var o1 = {a:10, b:20, c:30};
var o2 = {a:10, b:20, c:30};

// Make o's map a migration target.
o1.b = 10.23;
o.a;

%PrepareFunctionForOptimization(f);
f(o, true);
%OptimizeFunctionOnNextCall(f);
f(o, false);
assertOptimized(f);

// Deprecate o's new map again and update the feedback vector but don't migrate
// o.
o1.c = 20.23;
f(o1, true);
assertOptimized(f);

// We migrates o's map without deopting or bailing out.
f(o, false);
f(o1, false);
f(o2, false);
assertOptimized(f);
