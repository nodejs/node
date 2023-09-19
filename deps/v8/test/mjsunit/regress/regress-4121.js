// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan

function literals_sharing_test(warmup, optimize) {
  function closure() {
    // Ensure small array literals start in specific element kind mode.
    assertTrue(%HasSmiElements([]));
    assertTrue(%HasSmiElements([1]));
    assertTrue(%HasSmiElements([1, 2]));
    assertTrue(%HasDoubleElements([1.1]));
    assertTrue(%HasDoubleElements([1.1, 2]));

    var a = [1, 2, 3];
    if (warmup) {
      // Transition elements kind during warmup...
      assertTrue(%HasSmiElements(a));
      assertEquals(4, a.push(1.3));
    }
    // ... and ensure that the information about transitioning is
    // propagated to the next closure.
    assertTrue(%HasDoubleElements(a));
  };
  %PrepareFunctionForOptimization(closure);
  ;
  %EnsureFeedbackVectorForFunction(closure);
  if (optimize) %OptimizeFunctionOnNextCall(closure);
  closure();
}


function test() {
  var warmup = true;
  for (var i = 0; i < 3; i++) {
    print('iter: ' + i + ', warmup: ' + warmup);
    literals_sharing_test(warmup, false);
    warmup = false;
  }
  print("iter: " + i + ", opt: true");
  literals_sharing_test(warmup, true);
}

test();
