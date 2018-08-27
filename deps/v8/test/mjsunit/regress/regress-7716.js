// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Deeply nested target.

let proxy = new Proxy(function(){}, {});
for (let i = 0; i < 100000; i++) {
  proxy = new Proxy(proxy, {});
}

// We get a stack overflow in all cases except for Reflect.apply, which here
// happens to run in constant space: Call jumps into CallProxy and CallProxy
// jumps into the next Call.
assertDoesNotThrow(() => Reflect.apply(proxy, {}, []));
assertThrows(() => Reflect.construct(proxy, []), RangeError);
assertThrows(() => Reflect.defineProperty(proxy, "x", {}), RangeError);
assertThrows(() => Reflect.deleteProperty(proxy, "x"), RangeError);
assertThrows(() => Reflect.get(proxy, "x"), RangeError);
assertThrows(() => Reflect.getOwnPropertyDescriptor(proxy, "x"), RangeError);
assertThrows(() => Reflect.getPrototypeOf(proxy), RangeError);
assertThrows(() => Reflect.has(proxy, "x"), RangeError);
assertThrows(() => Reflect.isExtensible(proxy), RangeError);
assertThrows(() => Reflect.ownKeys(proxy), RangeError);
assertThrows(() => Reflect.preventExtensions(proxy), RangeError);
assertThrows(() => Reflect.setPrototypeOf(proxy, {}), RangeError);
assertThrows(() => Reflect.set(proxy, "x", {}), RangeError);


// Recursive handler.

function run(trap, ...args) {
  let handler = {};
  const proxy = new Proxy(function(){}, handler);
  handler[trap] = (target, ...args) => Reflect[trap](proxy, ...args);
  return Reflect[trap](proxy, ...args);
}

assertThrows(() => run("apply", {}, []), RangeError);
assertThrows(() => run("construct", []), RangeError);
assertThrows(() => run("defineProperty", "x", {}), RangeError);
assertThrows(() => run("deleteProperty", "x"), RangeError);
assertThrows(() => run("get", "x"), RangeError);
assertThrows(() => run("getOwnPropertyDescriptor", "x"), RangeError);
assertThrows(() => run("has", "x"), RangeError);
assertThrows(() => run("isExtensible"), RangeError);
assertThrows(() => run("ownKeys"), RangeError);
assertThrows(() => run("preventExtensions"), RangeError);
assertThrows(() => run("setPrototypeOf", {}), RangeError);
assertThrows(() => run("set", "x", {}), RangeError);
