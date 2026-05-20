// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var realm = Realm.create();

this.__proto__ = new Proxy({}, {
  getPrototypeOf() { assertUnreachable() },
  get() { assertUnreachable() }
});

var other_type_error = Realm.eval(realm, "TypeError");
assertThrows(() => Realm.eval(realm, "Realm.global(0).foo"), other_type_error);
