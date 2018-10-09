// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Deeply nested target.

let proxy = new Proxy(function(){}, {});
for (let i = 0; i < 100000; i++) {
  proxy = new Proxy(proxy, {});
}

// Ensure these nested calls don't segfault. They may not all throw exceptions
// depending on whether the compiler is able to perform tail call optimization
// on the affected routines.
try { Reflect.apply(proxy, {}, []) } catch(_) {}
try { Reflect.construct(proxy, []) } catch(_) {}
try { Reflect.defineProperty(proxy, "x", {}) } catch(_) {}
try { Reflect.deleteProperty(proxy, "x") } catch(_) {}
try { Reflect.get(proxy, "x") } catch(_) {}
try { Reflect.getOwnPropertyDescriptor(proxy, "x") } catch(_) {}
try { Reflect.getPrototypeOf(proxy) } catch(_) {}
try { Reflect.has(proxy, "x") } catch(_) {}
try { Reflect.isExtensible(proxy) } catch(_) {}
try { Reflect.ownKeys(proxy) } catch(_) {}
try { Reflect.preventExtensions(proxy) } catch(_) {}
try { Reflect.setPrototypeOf(proxy, {}) } catch(_) {}
try { Reflect.set(proxy, "x", {}) } catch(_) {}


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
