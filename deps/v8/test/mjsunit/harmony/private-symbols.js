// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


var symbol = %CreatePrivateSymbol("private");


// Private symbols must never be listed.

var object = {};
object[symbol] = 42;
for (var key of Object.keys(object)) assertUnreachable();
for (var key of Object.getOwnPropertySymbols(object)) assertUnreachable();
for (var key of Object.getOwnPropertyNames(object)) assertUnreachable();
for (var key of Reflect.ownKeys(object)) assertUnreachable();
for (var key in object) assertUnreachable();

var object2 = {__proto__: object};
for (var key of Object.keys(object2)) assertUnreachable();
for (var key of Object.getOwnPropertySymbols(object2)) assertUnreachable();
for (var key of Object.getOwnPropertyNames(object2)) assertUnreachable();
for (var key of Reflect.ownKeys(object2)) assertUnreachable();
for (var key in object2) assertUnreachable();


// Private symbols must never leak to proxy traps.

var proxy = new Proxy({}, new Proxy({}, {get() {return () => {throw 666}}}));
var object = {__proto__: proxy};

// [[Set]]
assertEquals(42, proxy[symbol] = 42);
assertThrows(function() { "use strict"; proxy[symbol] = 42 }, TypeError);
assertEquals(false, Reflect.set(proxy, symbol, 42));
assertEquals(42, object[symbol] = 42);
assertEquals(43, (function() {"use strict"; return object[symbol] = 43})());
assertEquals(true, Reflect.set(object, symbol, 44));

// [[DefineOwnProperty]]
assertEquals(false, Reflect.defineProperty(proxy, symbol, {}));
assertThrows(() => Object.defineProperty(proxy, symbol, {}), TypeError);
assertEquals(true, Reflect.defineProperty(object, symbol, {}));
assertEquals(object, Object.defineProperty(object, symbol, {}));

// [[Delete]]
assertEquals(true, delete proxy[symbol]);
assertEquals(true, (function() {"use strict"; return delete proxy[symbol]})());
assertEquals(true, Reflect.deleteProperty(proxy, symbol));
assertEquals(true, delete object[symbol]);
assertEquals(true, (function() {"use strict"; return delete object[symbol]})());
assertEquals(true, Reflect.deleteProperty(object, symbol));

// [[GetOwnPropertyDescriptor]]
assertEquals(undefined, Object.getOwnPropertyDescriptor(proxy, symbol));
assertEquals(undefined, Reflect.getOwnPropertyDescriptor(proxy, symbol));
assertFalse(Object.prototype.hasOwnProperty.call(proxy, symbol));
assertEquals(undefined, Object.getOwnPropertyDescriptor(object, symbol));
assertEquals(undefined, Reflect.getOwnPropertyDescriptor(object, symbol));
assertFalse(Object.prototype.hasOwnProperty.call(object, symbol));

// [[Has]]
assertFalse(symbol in proxy);
assertFalse(Reflect.has(proxy, symbol));
assertFalse(symbol in object);
assertFalse(Reflect.has(object, symbol));

// [[Get]]
assertEquals(undefined, proxy[symbol]);
assertEquals(undefined, Reflect.get(proxy, symbol));
assertEquals(undefined, Reflect.get(proxy, symbol, 42));
assertEquals(undefined, object[symbol]);
assertEquals(undefined, Reflect.get(object, symbol));
assertEquals(undefined, Reflect.get(object, symbol, 42));
