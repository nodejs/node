// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-proxies

"use strict";


var global = this;


(function TestGlobalReceiver() {
  class A {
    s() {
      super.bla = 10;
    }
  }
  new A().s.call(global);
  assertEquals(10, global.bla);
})();


(function TestProxyProto() {
  var calls = 0;
  var handler = {
    getPropertyDescriptor: function(name) {
      calls++;
      return undefined;
    }
  };

  var proto = {};
  var proxy = Proxy.create(handler, proto);
  var object = {
    __proto__: proxy,
    setX(v) {
      super.x = v;
    },
    setSymbol(sym, v) {
      super[sym] = v;
    }
  };

  object.setX(1);
  assertEquals(1, Object.getOwnPropertyDescriptor(object, 'x').value);
  assertEquals(1, calls);

  var sym = Symbol();
  object.setSymbol.call(global, sym, 2);
  assertEquals(2, Object.getOwnPropertyDescriptor(global, sym).value);
  // We currently do not invoke proxy traps for symbols
  assertEquals(1, calls);
})();


(function TestProxyReceiver() {
  var object = {
    setY(v) {
      super.y = v;
    }
  };

  var calls = 0;
  var handler = {
    getPropertyDescriptor(name) {
      assertUnreachable();
    },
    set(receiver, name, value) {
      calls++;
      assertEquals(proxy, receiver);
      assertEquals('y', name);
      assertEquals(3, value);
    }
  };

  var proxy = Proxy.create(handler);
  object.setY.call(proxy, 3);
  assertEquals(1, calls);
})();
