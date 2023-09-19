// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let a = [0, 1, 2, 3, 4];

function empty() {}

function f(p) {
  a.pop(Reflect.construct(empty, arguments, p));
}

let p = new Proxy(Object, {
    get: () => (a[0] = 1.1, Object.prototype)
});

function main(p) {
  f(p);
}

%PrepareFunctionForOptimization(empty);
%PrepareFunctionForOptimization(f);
%PrepareFunctionForOptimization(main);

main(empty);
main(empty);
%OptimizeFunctionOnNextCall(main);
main(p);
