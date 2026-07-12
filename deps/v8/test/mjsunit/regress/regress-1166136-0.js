// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --turbo-inlining

function main() {
  function vul(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9,
               arg10, arg11, arg12, arg13, arg14, arg15, arg16, arg17, arg18,
               arg19, arg20, arg21, arg22, arg23, arg24, arg25, arg26,) {
    let local_0 = Reflect.construct(Object,arguments,vul);
    let local_1;
    let local_2;
    let local_3;
    let local_4;
    let local_5;
    let local_6;
    let local_7;
    let local_8;
    let local_9;
    let local_10;
    let local_11;
    let local_12;
    let local_13;
    let local_14;
    let local_15;
    let local_16;
    let local_17;
    let local_18;
    let local_19;
    let local_20;
  }

  %PrepareFunctionForOptimization(vul);
  vul(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1,1);
}
%PrepareFunctionForOptimization(main);
main();
%OptimizeFunctionOnNextCall(main);
main();
