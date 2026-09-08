// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function probe(a) {
  let o = {};
  try {
    let proxy = new Proxy(a, o);
    Object.setPrototypeOf(a, proxy);
  } catch (e) {}
}

const o = { probe: probe };

function foo() {
  for (let i = (() => {
    const iterator = {
      [Symbol.iterator]() {
        o.probe();
        const res = {
          next() {
            const iterator = {
              [Symbol.iterator]() {
                let n = 10;
                const res = {
                  next() { return { done: n }; }
                };
                o.probe(res);
                return res;
              }
            };
            o.probe(iterator);
            for (const _ of iterator) {}
          }
        };
        return res;
      }
    };
    try {
      for (const _ of iterator) {}
    } catch (e) {}
    return 10;
  })(); i; i--) {}
  return foo;
}

function bar() {
  const S = foo();
  class C extends S {}
  new C();
}

%PrepareFunctionForOptimization(foo);
for (let i = 0; i < 3; i++) bar();
%OptimizeFunctionOnNextCall(foo);
bar();
