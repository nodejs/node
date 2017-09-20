// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = this;
;(function () {
  var calledDelete = false;
  var calledGet = false;
  var calledHas = false;
  var calledSet = false;
  var target = {};
  var assertEquals = global.assertEquals;
  var proxy = new Proxy(target, {
    has(target, property) {
      calledHas = true;
      return Reflect.has(target, property);
    },
    get(target, property, receiver) {
      calledGet = true;
      return Reflect.get(target, property, receiver);
    },
    set(targer, property, value, receiver) {
      calledSet = true;
      return Reflect.set(target, property, value, receiver);
    },
    delete(target, property, receiver) {
      calledDelete = true;
      return Reflect.delete(target, property, receiver);
    }
  });
  Object.setPrototypeOf(global, proxy);
  getGlobal;
  assertTrue(calledGet);
  makeGlobal = 2;
  assertTrue(calledSet);
  "findGlobal" in global;
  assertTrue(calledHas);
  var deletedOwn = delete makeGlobal;
  assertTrue(deletedOwn);
  assertEquals(void 0, makeGlobal);
})();
