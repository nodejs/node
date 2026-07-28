// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fast-proxy-ic

const proxy = new Proxy({}, {
  get(target, prop, receiver) {
    return prop;
  }
});


// Case 1: string key mismatch deopts
{
  function test1(p, key) {
    return p[key];
  }
  %PrepareFunctionForOptimization(test1);
  for (let i = 0; i < 1000; i++) {
    test1(proxy, "foo");
  }
  %OptimizeFunctionOnNextCall(test1);
  test1(proxy, "foo");
  const result = test1(proxy, "bar");
  assertEquals("bar", result);
}


// Case 2: numeric key deopts and converts to string
{
  function test2(p, key) {
    return p[key];
  }
  %PrepareFunctionForOptimization(test2);
  for (let i = 0; i < 1000; i++) {
    test2(proxy, "foo");
  }
  %OptimizeFunctionOnNextCall(test2);
  test2(proxy, "foo");
  const result = test2(proxy, 1);
  assertEquals("1", result);
}


// Case 3: double key deopts and converts to string
{
  function test3(p, key) {
    return p[key];
  }
  %PrepareFunctionForOptimization(test3);
  for (let i = 0; i < 1000; i++) {
    test3(proxy, "foo");
  }
  %OptimizeFunctionOnNextCall(test3);
  test3(proxy, "foo");
  const result = test3(proxy, 1.5);
  assertEquals("1.5", result);
}


// Case 4: boolean key deopts and converts to string
{
  function test4(p, key) {
    return p[key];
  }
  %PrepareFunctionForOptimization(test4);
  for (let i = 0; i < 1000; i++) {
    test4(proxy, "foo");
  }
  %OptimizeFunctionOnNextCall(test4);
  test4(proxy, "foo");
  const result = test4(proxy, true);
  assertEquals("true", result);
}


// Case 5: object key deopts and converts to string
{
  function test5(p, key) {
    return p[key];
  }
  %PrepareFunctionForOptimization(test5);
  for (let i = 0; i < 1000; i++) {
    test5(proxy, "foo");
  }
  %OptimizeFunctionOnNextCall(test5);
  test5(proxy, "foo");
  const obj = {};
  const result = test5(proxy, obj);
  assertEquals("[object Object]", result);
}
