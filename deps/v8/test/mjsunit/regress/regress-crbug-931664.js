// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt(){
  for(l in('a')){
    try{
      for(a in('')) {
        for(let arg2 in(+(arg2)));
      }
    }
    finally{}
  }
}
%PrepareFunctionForOptimization(opt);
opt();
%OptimizeFunctionOnNextCall(opt);
opt();
