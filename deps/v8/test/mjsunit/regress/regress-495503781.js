// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Realm.createAllowCrossRealmAccess();
let f_1 = Realm.eval(1, "function f() {} f");
f_1.prototype = 42;
Realm.navigate(1);

let instant_0 = Reflect.construct(Temporal.Instant, [700000000000000000n]);
let instant_1 = Reflect.construct(Temporal.Instant, [700000000000000000n], f_1);

assertEquals(typeof(instant_0.constructor), "function");
assertEquals(typeof(instant_1.constructor), "function");
assertNotEquals(instant_0.constructor, instant_1.constructor);

let s_0 = instant_0.toString();
let s_1 = instant_1.toString();
assertEquals(s_0, s_1);
