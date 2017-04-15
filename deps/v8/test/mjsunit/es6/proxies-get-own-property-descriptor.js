// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var target = {};
var configurable_desc = {
  value: 123,
  configurable: true,
  writable: true,
  enumerable: false,
};
Object.defineProperty(target, "configurable", configurable_desc);
var nonconfigurable_desc = {
  value: 234,
  configurable: false,
  writable: false,
  enumerable: true
}
Object.defineProperty(target, "nonconfigurable", nonconfigurable_desc);

var proxied_desc = {
  value: 345,
  configurable: true
};

var handler = {
  "getOwnPropertyDescriptor": function(target, name) {
    if (name === "proxied") {
      return proxied_desc;
    }
    if (name === "return_null") {
      return null;
    }
    return Object.getOwnPropertyDescriptor(target, name);
  }
};

var proxy = new Proxy(target, handler);
var proxy_without_handler = new Proxy(target, {});

// Checking basic functionality:

assertEquals(configurable_desc,
             Object.getOwnPropertyDescriptor(proxy, "configurable"));
assertEquals(nonconfigurable_desc,
             Object.getOwnPropertyDescriptor(proxy, "nonconfigurable"));
assertEquals({ value: proxied_desc.value,
               configurable: proxied_desc.configurable,
               enumerable: false,
               writable: false },
             Object.getOwnPropertyDescriptor(proxy, "proxied"));
assertEquals(configurable_desc,
             Object.getOwnPropertyDescriptor(proxy_without_handler,
                                             "configurable"));
assertEquals(nonconfigurable_desc,
             Object.getOwnPropertyDescriptor(proxy_without_handler,
                                             "nonconfigurable"));

assertThrows('Object.getOwnPropertyDescriptor(proxy, "return_null")');

handler.getOwnPropertyDescriptor = undefined;
assertEquals(configurable_desc,
             Object.getOwnPropertyDescriptor(proxy, "configurable"));

// Checking invariants mentioned explicitly by the ES spec:

// (Inv-1) "A property cannot be reported as non-existent, if it exists as a
// non-configurable own property of the target object."
handler.getOwnPropertyDescriptor = function(target, name) { return undefined; };
assertThrows('Object.getOwnPropertyDescriptor(proxy, "nonconfigurable")');
assertEquals(undefined, Object.getOwnPropertyDescriptor(proxy, "configurable"));

// (Inv-2) "A property cannot be reported as non-configurable, if it does not
// exist as an own property of the target object or if it exists as a
// configurable own property of the target object."
handler.getOwnPropertyDescriptor = function(target, name) {
  return {value: 234, configurable: false, enumerable: true};
};
assertThrows('Object.getOwnPropertyDescriptor(proxy, "nonexistent")');
assertThrows('Object.getOwnPropertyDescriptor(proxy, "configurable")');
assertEquals(
    false,
    Object.getOwnPropertyDescriptor(proxy, "nonconfigurable").configurable);

// (Inv-3) "A property cannot be reported as non-existent, if it exists as an
// own property of the target object and the target object is not extensible."
Object.seal(target);
handler.getOwnPropertyDescriptor = function(target, name) { return undefined; };
assertThrows('Object.getOwnPropertyDescriptor(proxy, "configurable")');
assertThrows('Object.getOwnPropertyDescriptor(proxy, "nonconfigurable")');
assertEquals(undefined, Object.getOwnPropertyDescriptor(proxy, "nonexistent"));

// (Inv-4) "A property cannot be reported as existent, if it does not exist as
// an own property of the target object and the target object is not
// extensible."
var existent_desc = {value: "yes"};
handler.getOwnPropertyDescriptor = function() { return existent_desc; };
assertThrows('Object.getOwnPropertyDescriptor(proxy, "nonexistent")');
assertEquals(
    {value: "yes", writable: false, enumerable: false, configurable: false},
    Object.getOwnPropertyDescriptor(proxy, "configurable"));

// Checking individual bailout points in the implementation:

// Step 6: Trap is not callable.
handler.getOwnPropertyDescriptor = {};
assertThrows('Object.getOwnPropertyDescriptor(proxy, "configurable")');

// Step 8: Trap throws.
handler.getOwnPropertyDescriptor = function() { throw "ball"; };
assertThrows('Object.getOwnPropertyDescriptor(proxy, "configurable")');

// Step 9: Trap result is neither undefined nor an object.
handler.getOwnPropertyDescriptor = function() { return 1; }
assertThrows('Object.getOwnPropertyDescriptor(proxy, "configurable")');

// Step 11b: See (Inv-1) above.
// Step 11e: See (Inv-3) above.

// Step 16: Incompatible PropertyDescriptor; a non-configurable property
// cannot be reported as configurable. (Inv-4) above checks more cases.
handler.getOwnPropertyDescriptor = function(target, name) {
  return {value: 456, configurable: true, writable: true}
};
assertThrows('Object.getOwnPropertyDescriptor(proxy, "nonconfigurable")');

// Step 17: See (Inv-2) above.
