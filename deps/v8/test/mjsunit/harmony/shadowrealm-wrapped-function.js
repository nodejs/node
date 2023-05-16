// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

var shadowRealm = new ShadowRealm();

// create a proxy on the wrapped value
var wrapped = shadowRealm.evaluate('() => 1');
assertEquals(wrapped(), 1);
var proxy = new Proxy(wrapped, {
  call: function(target, thisArg, args) {
    assertEquals(target, wrapped);
    return target();
  },
});
assertEquals(proxy(), 1);

// create a revocable proxy on the wrapped value
var revocable = Proxy.revocable(wrapped, {
  call: function(target, thisArg, args) {
    assertEquals(target, wrapped);
    return target();
  },
});
var proxy = revocable.proxy;
assertEquals(proxy(), 1);
revocable.revoke();
assertThrows(() => proxy(), TypeError, "Cannot perform 'apply' on a proxy that has been revoked");

// no-side-effects inspection on thrown error
var wrapped = shadowRealm.evaluate(`() => {
  throw new Error('foo');
}`);
assertThrows(() => wrapped(), TypeError, "WrappedFunction threw (Error: foo)");

// no-side-effects inspection on thrown error
var wrapped = shadowRealm.evaluate(`
globalThis.messageAccessed = false;
() => {
  const err = new Error('foo');
  Object.defineProperty(err, 'message', {
    get: function() {
      globalThis.messageAccessed = true;
      return 'bar';
    },
  });
  throw err;
}
`);
assertThrows(() => wrapped(), TypeError, "WrappedFunction threw (Error)");
assertFalse(shadowRealm.evaluate('globalThis.messageAccessed'));
