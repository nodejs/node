// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function TestBasics() {
  var log = [];

  var proxy = new Proxy({}, {
    get: function(target, key) {
      log.push("get " + String(key));
      if (key === 'x') return 1;
    },
    has: function(target, key) {
      log.push("has " + String(key));
      if (key === 'x') return true;
      return false;
    }
  });

  var x = 'local';

  with (proxy) {
    assertEquals(1, x);
  }

  assertEquals(['has assertEquals', 'has x', 'get Symbol(Symbol.unscopables)',
                'get x'], log);
}
TestBasics();


function TestInconsistent() {
  var log = [];

  var proxy = new Proxy({}, {
    get: function(target, key) {
      log.push("get " + String(key));
      return undefined;
    },
    has: function(target, key) {
      log.push("has " + String(key));
      if (key === 'x') return true;
      return false;
    }
  });

  var x = 'local';

  with (proxy) {
    assertEquals(void 0, x);
  }

  assertEquals(['has assertEquals', 'has x', 'get Symbol(Symbol.unscopables)',
                'get x'], log);
}
TestInconsistent();


function TestUseProxyAsUnscopables() {
  var x = 1;
  var object = {
    x: 2
  };
  var calls = 0;
  var proxy = new Proxy({}, {
    has: function() {
      assertUnreachable();
    },
    get: function(target, key) {
      assertEquals('x', key);
      calls++;
      return calls === 2 ? true : undefined;
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
  var proxy = new Proxy({}, {
    has: function() {
      assertUnreachable();
    },
    get: function(target, key) {
      if (calls++ === 0) {
        throw new CustomError();
      }
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
  var proxy = new Proxy({}, {
    get: function() {
      assertUnreachable();
    },
    has: function() {
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
