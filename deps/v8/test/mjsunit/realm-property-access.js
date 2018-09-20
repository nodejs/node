// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var r = Realm.create();
var f = Realm.eval(r, "function f() { return this }; f()");
assertEquals(f, Realm.global(r));

// Cross-origin property access throws
assertThrows(() => f.a, TypeError);
assertThrows(() => { 'use strict'; f.a = 1 }, TypeError);

var r2 = Realm.createAllowCrossRealmAccess();
var f2 = Realm.eval(r2, "function f() { return this }; f()");
assertEquals(f2, Realm.global(r2));

// Same-origin property access doesn't throw
assertEquals(undefined, f2.a);
f2.a = 1;
assertEquals(1, f2.a);
