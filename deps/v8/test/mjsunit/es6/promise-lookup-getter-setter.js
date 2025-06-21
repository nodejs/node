// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let get = () => {};
let set = () => {};
let target = {};
let handler = {
  getOwnPropertyDescriptor(target, prop) {
    let configurable = true;
    if (prop == "both") {
      return { get, set, configurable };
    } else if (prop == "get") {
      return { get, configurable };
    } else if (prop == "set") {
      return { set, configurable };
    } else if (prop == "data") {
      return { value: 42, configurable };
    } else {
      return Reflect.getOwnPropertyDescriptor(target, prop);
    }
  }
};

// Test behavior on own properties.
let proxy = new Proxy(target, handler);
assertSame(get, proxy.__lookupGetter__("both"));
assertSame(get, proxy.__lookupGetter__("get"));
assertSame(undefined, proxy.__lookupGetter__("set"));
assertSame(undefined, proxy.__lookupGetter__("data"));
assertSame(set, proxy.__lookupSetter__("both"));
assertSame(undefined, proxy.__lookupSetter__("get"));
assertSame(set, proxy.__lookupSetter__("set"));
assertSame(undefined, proxy.__lookupSetter__("data"));

// Test behavior on the prototype chain.
let object = { __proto__: proxy };
assertSame(get, object.__lookupGetter__("both"));
assertSame(get, object.__lookupGetter__("get"));
assertSame(undefined, object.__lookupGetter__("set"));
assertSame(undefined, object.__lookupGetter__("data"));
assertSame(set, object.__lookupSetter__("both"));
assertSame(undefined, object.__lookupSetter__("get"));
assertSame(set, object.__lookupSetter__("set"));
assertSame(undefined, object.__lookupSetter__("data"));

// Test being shadowed while on prototype chain.
let shadower = { __proto__: proxy, both: 1, get: 2, set: 3, data: 4 };
assertSame(undefined, shadower.__lookupGetter__("both"));
assertSame(undefined, shadower.__lookupGetter__("get"));
assertSame(undefined, shadower.__lookupGetter__("set"));
assertSame(undefined, shadower.__lookupGetter__("data"));
assertSame(undefined, shadower.__lookupSetter__("both"));
assertSame(undefined, shadower.__lookupSetter__("get"));
assertSame(undefined, shadower.__lookupSetter__("set"));
assertSame(undefined, shadower.__lookupSetter__("data"));

// Test getPrototypeOf trap.
let getFoo = () => {};
let setFoo = () => {};
let proto = {};
Reflect.defineProperty(proto, "foo", { get: getFoo, set: setFoo });
Reflect.setPrototypeOf(target, proto);
assertSame(getFoo, proxy.__lookupGetter__("foo"));
assertSame(setFoo, proxy.__lookupSetter__("foo"));
handler.getPrototypeOf = () => null;
assertSame(undefined, proxy.__lookupGetter__("foo"));
assertSame(undefined, proxy.__lookupSetter__("foo"));
handler.getPrototypeOf = () => proto;
assertSame(getFoo, proxy.__lookupGetter__("foo"));
assertSame(setFoo, proxy.__lookupSetter__("foo"));

// Test shadowing the prototype.
Reflect.defineProperty(proto, "data", { get: getFoo, set: setFoo });
assertSame(undefined, proxy.__lookupGetter__("data"));
assertSame(undefined, proxy.__lookupSetter__("data"));
