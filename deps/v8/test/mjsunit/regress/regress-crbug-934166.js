// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

{
  function f() {
    for(let i = 0; i < 10; ++i){
      try{
        // Carefully constructed by a fuzzer to use a new register for s(), whose
        // write is dead due to the unconditional throw after s()=N, but which is
        // read in the ({...g}) call, which therefore must also be marked dead and
        // elided.
        with(f&&g&&(s()=N)({...g})){}
      } catch {}
      %OptimizeOsr();
    }
  }
  %PrepareFunctionForOptimization(f);
  f();
}
