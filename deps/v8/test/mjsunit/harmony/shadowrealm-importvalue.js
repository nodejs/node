// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm --allow-natives-syntax

var shadowRealm = new ShadowRealm();
globalThis.foobar = 'outer-scope';

{
  const promise = shadowRealm.importValue('./shadowrealm-skip-1.mjs', 'func');
  // Promise is created in caller realm.
  assertInstanceof(promise, Promise);

  assertPromiseResult(promise.then(func => {
    // Check that side-effects in the ShadowRealm not propagated to the caller.
    assertEquals(globalThis.foobar, 'outer-scope');
    // Check that the func is created in the current Realm.
    assertEquals(typeof func, 'function');
    assertTrue(func instanceof Function);
    // Should return the inner global value.
    assertEquals(func(), 'inner-scope');
  }));
}

{
  const promise = shadowRealm.importValue('./shadowrealm-skip-1.mjs', 'foo');
  // Promise is created in caller realm.
  assertInstanceof(promise, Promise);

  assertPromiseResult(promise.then(foo => {
    assertEquals(foo, 'bar');
  }));
}

{
  const promise = shadowRealm.importValue('./shadowrealm-skip-1.mjs', 'obj');
  // Promise is created in caller realm.
  assertInstanceof(promise, Promise);

  assertThrowsAsync(promise, TypeError, "[object Object] is not a function");
}

{
  const promise = shadowRealm.importValue('./shadowrealm-skip-1.mjs', 'not_exists');
  // Promise is created in caller realm.
  assertInstanceof(promise, Promise);
  assertThrowsAsync(promise, TypeError, "The requested module './shadowrealm-skip-1.mjs' does not provide an export named 'not_exists'");
}

{
  const promise = shadowRealm.importValue('./shadowrealm-skip-not-found.mjs', 'foo');
  // Promise is created in caller realm.
  assertInstanceof(promise, Promise);
  assertThrowsAsync(promise, TypeError, /Cannot import in ShadowRealm \(Error: .+shadowrealm-skip-not-found\.mjs\)/);
}

{
  const promise = shadowRealm.importValue('./shadowrealm-skip-2-throw.mjs', 'foo');
  // Promise is created in caller realm.
  assertInstanceof(promise, Promise);
  assertThrowsAsync(promise, TypeError, 'Cannot import in ShadowRealm (Error: foobar)');
}

// no-side-effects inspection on thrown error
{
  const promise = shadowRealm.importValue('./shadowrealm-skip-3-throw-object.mjs', 'foo');
  // Promise is created in caller realm.
  assertInstanceof(promise, Promise);
  assertThrowsAsync(promise, TypeError, 'Cannot import in ShadowRealm ([object Object])');
}

// Invalid args
assertThrows(() => ShadowRealm.prototype.importValue.call(1, '', ''), TypeError, 'Method ShadowRealm.prototype.importValue called on incompatible receiver 1')
assertThrows(() => ShadowRealm.prototype.importValue.call({}, '', ''), TypeError, 'Method ShadowRealm.prototype.importValue called on incompatible receiver #<Object>')
