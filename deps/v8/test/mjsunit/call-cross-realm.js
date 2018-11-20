// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Realm.create();
var object = Realm.eval(1, "Object");
var f = Realm.eval(1, "function f() { return this }; f");

Number.prototype.f = f;
var number = 1;
assertEquals(object.prototype, f.call(number).__proto__.__proto__);
assertEquals(object.prototype, number.f().__proto__.__proto__);
assertEquals(Realm.global(1), f());
