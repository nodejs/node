// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


traps = [
    "getPrototypeOf", "setPrototypeOf", "isExtensible", "preventExtensions",
    "getOwnPropertyDescriptor", "has", "get", "set", "deleteProperty",
    "defineProperty", "ownKeys", "apply", "construct"
];

var {proxy, revoke} = Proxy.revocable({}, {});
assertEquals(0, revoke.length);

assertEquals(undefined, revoke());
for (var trap of traps) {
  assertThrows(() => Reflect[trap](proxy), TypeError);
}

assertEquals(undefined, revoke());
for (var trap of traps) {
  assertThrows(() => Reflect[trap](proxy), TypeError);
}

// Throw TypeError if target or handler is revoked proxy
var revocable = Proxy.revocable({}, {});
revocable.revoke();
assertThrows(function(){ Proxy.revocable(revocable.proxy, {}); }, TypeError);
assertThrows(function(){ Proxy.revocable({}, revocable.proxy); }, TypeError);
