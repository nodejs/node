// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const realm = Realm.create();
const victimProxy = Realm.global(realm);

Realm.eval(realm, `
  Error.captureStackTrace(globalThis);
  globalThis.stack = "secret-realm-stack";
`);

const accessChecked = d8.test.createAccessCheckedObject(true);
Error.captureStackTrace(accessChecked);
accessChecked.stack = "secret-object-stack";
d8.test.setAccessPolicy(accessChecked, false);

const localTarget = {};
Error.captureStackTrace(localTarget);
const customDesc = Object.getOwnPropertyDescriptor(localTarget, "stack");

const errorInstance = new Error();
const errorDesc = Object.getOwnPropertyDescriptor(errorInstance, "stack");

// Same-origin globalThis.stack still works on JSGlobalProxy.
Error.captureStackTrace(globalThis);
globalThis.stack = "same-origin-stack";
assertEquals("same-origin-stack", globalThis.stack);
assertEquals("same-origin-stack", customDesc.get.call(globalThis));

for (const [holder, desc] of [[localTarget, customDesc], [errorInstance, errorDesc]]) {
  for (const target of [victimProxy, accessChecked]) {
    // Direct getter/setter invocation.
    assertThrows(() => desc.get.call(target), TypeError);
    assertThrows(() => Reflect.apply(desc.get, target, []), TypeError);
    assertThrows(() => desc.set.call(target, "clobbered"), TypeError);
    assertThrows(() => Reflect.apply(desc.set, target, ["clobbered"]), TypeError);

    // Receiver-substituted property access.
    assertThrows(() => Reflect.get(holder, "stack", target), TypeError);
    assertThrows(() => Reflect.set(holder, "stack", "clobbered", target), TypeError);
  }
}

// Verify values were not modified.
assertEquals("secret-realm-stack", Realm.eval(realm, "globalThis.stack"));
d8.test.setAccessPolicy(accessChecked, true);
assertEquals("secret-object-stack", accessChecked.stack);
