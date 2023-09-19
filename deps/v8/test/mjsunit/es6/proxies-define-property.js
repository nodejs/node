// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check basic call to trap.

var g_target, g_name, g_desc;
var handler = {
  defineProperty: function(target, name, desc) {
    g_target = target;
    g_name = name;
    g_desc = desc;
    return true;
  }
}
var target = {}
var proxy = new Proxy(target, handler);
var desc = { value: 1, writable: true, configurable: true, enumerable: true };
Object.defineProperty(proxy, "foo", desc);
assertSame(target, g_target);
assertEquals("foo", g_name);
assertEquals(desc, g_desc);

// Check specific steps in the spec

// Step 4: revoked handler
var pair = Proxy.revocable(target, handler);
Object.defineProperty(proxy, "foo2", desc);
assertSame(target, g_target);
assertEquals("foo2", g_name);
assertEquals(desc, g_desc);
pair.revoke();
assertThrows('Object.defineProperty(pair.proxy, "bar", desc);', TypeError);

// Step 6: Trap isn't callable.
handler.defineProperty = 1;
assertThrows("Object.defineProperty(proxy, 'foo', {value: 2})", TypeError);

// Step 7: Trap is undefined.
handler.defineProperty = undefined;
Object.defineProperty(proxy, "prop1", desc);
assertEquals(desc, Object.getOwnPropertyDescriptor(target, "prop1"));
var target2 = {};
var proxy2 = new Proxy(target2, {});
Object.defineProperty(proxy2, "prop2", desc);
assertEquals(desc, Object.getOwnPropertyDescriptor(target2, "prop2"));

// Step 9: Property name is passed to the trap as a string.
handler.defineProperty = function(t, name, d) { g_name = name; return true; };
Object.defineProperty(proxy, 0, desc);
assertTrue(typeof g_name === "string");
assertEquals("0", g_name);

// Step 10: Trap returns false.
handler.defineProperty = function(t, n, d) { return false; }
assertThrows("Object.defineProperty(proxy, 'foo', desc)", TypeError);

// Step 15a: Trap returns true for adding a property to a non-extensible target.
handler.defineProperty = function(t, n, d) { return true; }
Object.preventExtensions(target);
assertThrows("Object.defineProperty(proxy, 'foo', desc)", TypeError);

// Step 15b: Trap returns true for adding a non-configurable property.
target = {};
proxy = new Proxy(target, handler);
desc = {value: 1, writable: true, configurable: false, enumerable: true};
assertThrows("Object.defineProperty(proxy, 'foo', desc)", TypeError);
// No exception is thrown if a non-configurable property exists on the target.
Object.defineProperty(target, "nonconf",
                      {value: 1, writable: true, configurable: false});
Object.defineProperty(proxy, "nonconf", {value: 2, configurable: false});

// Step 16a: Trap returns true for non-compatible property descriptor.
Object.defineProperty(target, "foo",
                      {value: 1, writable: false, configurable: false});
assertThrows("Object.defineProperty(proxy, 'foo', {value: 2})", TypeError);

// Step 16b: Trap returns true for overwriting a configurable property
// with a non-configurable descriptor.
target.bar = "baz";
assertThrows("Object.defineProperty(proxy, 'bar', {configurable: false})",
             TypeError);
