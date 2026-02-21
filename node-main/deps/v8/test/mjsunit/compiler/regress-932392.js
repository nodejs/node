// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt(flag){
  ((flag||(Math.max(-0,0)))==0)
}

%PrepareFunctionForOptimization(opt);
try{opt(false)}catch{}
%OptimizeFunctionOnNextCall(opt)
try{opt(false)}catch{}
