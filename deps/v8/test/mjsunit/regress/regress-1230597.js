// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function main() {
  // This seems to just be some register setup for the benefit of the optimising
  // compiler.
  for(var i=0; i<2; i++) {
    if(i==1) { }
    with ({}) {
      try {
        var07().foo = 1;
      } catch { }
    }
  }

  // This is where the bug was.
  try {
    // The `var10() = ...` is rewritten to `var10[throw ReferenceError] = ...`,
    // making the value of the assign dead. However, there is a loop inside the
    // spread that used to accidentally resurrect that block.
    //
    // This broke checks in the optimizing compiler since the iterator setup was
    // dead but the iterator use inside the spread's loop was live.
    var10() = var08(1, ...foo, 2);
  } catch { }
}


%PrepareFunctionForOptimization(main);
main();
%OptimizeFunctionOnNextCall(main);
main();
