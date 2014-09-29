// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-unscopables
// Flags: --harmony-proxies


// TODO(arv): Once proxies can intercept symbols, add more tests.


function TestBasics() {
  var log = [];

  var proxy = Proxy.create({
    getPropertyDescriptor: function(key) {
      log.push(key);
      if (key === 'x') {
        return {
          value: 1,
          configurable: true
        };
      }
      return undefined;
    }
  });

  var x = 'local';

  with (proxy) {
    assertEquals(1, x);
  }

  // One 'x' for HasBinding and one for GetBindingValue
  assertEquals(['assertEquals', 'x', 'x'], log);
}
TestBasics();


function TestInconsistent() {
  var log = [];
  var calls = 0;

  var proxy = Proxy.create({
    getPropertyDescriptor: function(key) {
      log.push(key);
      if (key === 'x' && calls < 1) {
        calls++;
        return {
          value: 1,
          configurable: true
        };
      }
      return undefined;
    }
  });

  var x = 'local';

  with (proxy) {
    assertEquals(void 0, x);
  }

  // One 'x' for HasBinding and one for GetBindingValue
  assertEquals(['assertEquals', 'x', 'x'], log);
}
TestInconsistent();


function TestUseProxyAsUnscopables() {
  var x = 1;
  var object = {
    x: 2
  };
  var calls = 0;
  var proxy = Proxy.create({
    has: function(key) {
      calls++;
      assertEquals('x', key);
      return calls === 2;
    },
    getPropertyDescriptor: function(key) {
      assertUnreachable();
    }
  });

  object[Symbol.unscopables] = proxy;

  with (object) {
    assertEquals(2, x);
    assertEquals(1, x);
  }

  // HasBinding, HasBinding
  assertEquals(2, calls);
}
TestUseProxyAsUnscopables();


function TestThrowInHasUnscopables() {
  var x = 1;
  var object = {
    x: 2
  };

  function CustomError() {}

  var calls = 0;
  var proxy = Proxy.create({
    has: function(key) {
      if (calls++ === 0) {
        throw new CustomError();
      }
      assertUnreachable();
    },
    getPropertyDescriptor: function(key) {
      assertUnreachable();
    }
  });

  object[Symbol.unscopables] = proxy;

  assertThrows(function() {
    with (object) {
      x;
    }
  }, CustomError);
}
TestThrowInHasUnscopables();


var global = this;
function TestGlobalShouldIgnoreUnscopables() {
  global.x = 1;
  var proxy = Proxy.create({
    getPropertyDescriptor: function() {
      assertUnreachable();
    }
  });
  global[Symbol.unscopables] = proxy;

  assertEquals(1, global.x);
  assertEquals(1, x);

  global.x = 2;
  assertEquals(2, global.x);
  assertEquals(2, x);

  x = 3;
  assertEquals(3, global.x);
  assertEquals(3, x);
}
TestGlobalShouldIgnoreUnscopables();
