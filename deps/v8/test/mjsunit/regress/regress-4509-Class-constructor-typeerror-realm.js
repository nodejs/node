// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var realm = Realm.create();
var OtherTypeError = Realm.eval(realm, 'TypeError');

class Derived extends Object {
  constructor() {
    return null;
  }
}

assertThrows(() => { new Derived() }, TypeError);

var OtherDerived = Realm.eval(realm,
   "'use strict';" +
   "class Derived extends Object {" +
      "constructor() {" +
        "return null;" +
      "}};");

// Before throwing the TypeError we have to switch to the caller context.
assertThrows(() => { new OtherDerived() }, TypeError);
