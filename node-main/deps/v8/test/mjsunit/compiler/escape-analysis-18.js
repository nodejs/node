// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function bar(arr) {
    var x = 0;
    arr.forEach(function(el) {
        x = el;
    });
    return x;
}

function foo(array) {
    return bar(array);
}

%PrepareFunctionForOptimization(foo);
let array = [,.5,];
foo(array);
foo(array);
%OptimizeFunctionOnNextCall(foo);
foo(array);
