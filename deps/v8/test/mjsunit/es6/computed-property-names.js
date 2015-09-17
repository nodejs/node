// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function ID(x) {
  return x;
}



(function TestBasicsString() {
  var object = {
    a: 'A',
    ['b']: 'B',
    c: 'C',
    [ID('d')]: 'D',
  };
  assertEquals('A', object.a);
  assertEquals('B', object.b);
  assertEquals('C', object.c);
  assertEquals('D', object.d);
  assertArrayEquals(['a', 'b', 'c', 'd'], Object.keys(object));
})();


(function TestBasicsNumber() {
  var object = {
    a: 'A',
    [1]: 'B',
    c: 'C',
    [ID(2)]: 'D',
  };
  assertEquals('A', object.a);
  assertEquals('B', object[1]);
  assertEquals('C', object.c);
  assertEquals('D', object[2]);
  // Array indexes first.
  assertArrayEquals(['1', '2', 'a', 'c'], Object.keys(object));
})();


(function TestBasicsSymbol() {
  var sym1 = Symbol();
  var sym2 = Symbol();
  var object = {
    a: 'A',
    [sym1]: 'B',
    c: 'C',
    [ID(sym2)]: 'D',
  };
  assertEquals('A', object.a);
  assertEquals('B', object[sym1]);
  assertEquals('C', object.c);
  assertEquals('D', object[sym2]);
  assertArrayEquals(['a', 'c'], Object.keys(object));
  assertArrayEquals([sym1, sym2], Object.getOwnPropertySymbols(object));
})();


(function TestToNameSideEffects() {
  var counter = 0;
  var key1 = {
    toString: function() {
      assertEquals(0, counter++);
      return 'b';
    }
  };
  var key2 = {
    toString: function() {
      assertEquals(1, counter++);
      return 'd';
    }
  };
  var object = {
    a: 'A',
    [key1]: 'B',
    c: 'C',
    [key2]: 'D',
  };
  assertEquals(2, counter);
  assertEquals('A', object.a);
  assertEquals('B', object.b);
  assertEquals('C', object.c);
  assertEquals('D', object.d);
  assertArrayEquals(['a', 'b', 'c', 'd'], Object.keys(object));
})();


(function TestToNameSideEffectsNumbers() {
  var counter = 0;
  var key1 = {
    valueOf: function() {
      assertEquals(0, counter++);
      return 1;
    },
    toString: null
  };
  var key2 = {
    valueOf: function() {
      assertEquals(1, counter++);
      return 2;
    },
    toString: null
  };

  var object = {
    a: 'A',
    [key1]: 'B',
    c: 'C',
    [key2]: 'D',
  };
  assertEquals(2, counter);
  assertEquals('A', object.a);
  assertEquals('B', object[1]);
  assertEquals('C', object.c);
  assertEquals('D', object[2]);
  // Array indexes first.
  assertArrayEquals(['1', '2', 'a', 'c'], Object.keys(object));
})();


(function TestDoubleName() {
  var object = {
    [1.2]: 'A',
    [1e55]: 'B',
    [0.000001]: 'C',
    [-0]: 'D',
    // TODO(arv): https://code.google.com/p/v8/issues/detail?id=3815
    // [Infinity]: 'E',
    // [-Infinity]: 'F',
    [NaN]: 'G',
  };
  assertEquals('A', object['1.2']);
  assertEquals('B', object['1e+55']);
  assertEquals('C', object['0.000001']);
  assertEquals('D', object[0]);
  // TODO(arv): https://code.google.com/p/v8/issues/detail?id=3815
  // assertEquals('E', object[Infinity]);
  // assertEquals('F', object[-Infinity]);
  assertEquals('G', object[NaN]);
})();


(function TestGetter() {
  var object = {
    get ['a']() {
      return 'A';
    }
  };
  assertEquals('A', object.a);

  object = {
    get b() {
      assertUnreachable();
    },
    get ['b']() {
      return 'B';
    }
  };
  assertEquals('B', object.b);

  object = {
    get c() {
      assertUnreachable();
    },
    get ['c']() {
      assertUnreachable();
    },
    get ['c']() {
      return 'C';
    }
  };
  assertEquals('C', object.c);

  object = {
    get ['d']() {
      assertUnreachable();
    },
    get d() {
      return 'D';
    }
  };
  assertEquals('D', object.d);
})();


(function TestSetter() {
  var calls = 0;
  var object = {
    set ['a'](_) {
      calls++;
    }
  };
  object.a = 'A';
  assertEquals(1, calls);

  calls = 0;
  object = {
    set b(_) {
      assertUnreachable();
    },
    set ['b'](_) {
      calls++;
    }
  };
  object.b = 'B';
  assertEquals(1, calls);

  calls = 0;
  object = {
    set c(_) {
      assertUnreachable()
    },
    set ['c'](_) {
      assertUnreachable()
    },
    set ['c'](_) {
      calls++
    }
  };
  object.c = 'C';
  assertEquals(1, calls);

  calls = 0;
  object = {
    set ['d'](_) {
      assertUnreachable()
    },
    set d(_) {
      calls++
    }
  };
  object.d = 'D';
  assertEquals(1, calls);
})();


(function TestDuplicateKeys() {
  'use strict';
  // ES5 does not allow duplicate keys.
  // ES6 does but we haven't changed our code yet.

  var object = {
    a: 1,
    ['a']: 2,
  };
  assertEquals(2, object.a);
})();


(function TestProto() {
  var proto = {};
  var object = {
    __proto__: proto
  };
  assertEquals(proto, Object.getPrototypeOf(object));

  object = {
    '__proto__': proto
  };
  assertEquals(proto, Object.getPrototypeOf(object));

  object = {
    ['__proto__']: proto
  };
  assertEquals(Object.prototype, Object.getPrototypeOf(object));
  assertEquals(proto, object.__proto__);
  assertTrue(object.hasOwnProperty('__proto__'));

  object = {
    [ID('x')]: 'X',
    __proto__: proto
  };
  assertEquals('X', object.x);
  assertEquals(proto, Object.getPrototypeOf(object));
})();


(function TestExceptionInName() {
  function MyError() {};
  function throwMyError() {
    throw new MyError();
  }
  assertThrows(function() {
    var o = {
      [throwMyError()]: 42
    };
  }, MyError);
  assertThrows(function() {
    var o = {
      get [throwMyError()]() { return 42; }
    };
  }, MyError);
  assertThrows(function() {
    var o = {
      set [throwMyError()](_) { }
    };
  }, MyError);
})();


(function TestNestedLiterals() {
  var array = [
    42,
    { a: 'A',
      ['b']: 'B',
      c: 'C',
      [ID('d')]: 'D',
    },
    43,
  ];
  assertEquals(42, array[0]);
  assertEquals(43, array[2]);
  assertEquals('A', array[1].a);
  assertEquals('B', array[1].b);
  assertEquals('C', array[1].c);
  assertEquals('D', array[1].d);
  var object = {
    outer: 42,
    inner: {
      a: 'A',
      ['b']: 'B',
      c: 'C',
      [ID('d')]: 'D',
    },
    outer2: 43,
  };
  assertEquals(42, object.outer);
  assertEquals(43, object.outer2);
  assertEquals('A', object.inner.a);
  assertEquals('B', object.inner.b);
  assertEquals('C', object.inner.c);
  assertEquals('D', object.inner.d);
  var object = {
    outer: 42,
    array: [
      43,
      { a: 'A',
        ['b']: 'B',
        c: 'C',
        [ID('d')]: 'D',
      },
      44,
    ],
    outer2: 45
  };
  assertEquals(42, object.outer);
  assertEquals(45, object.outer2);
  assertEquals(43, object.array[0]);
  assertEquals(44, object.array[2]);
  assertEquals('A', object.array[1].a);
  assertEquals('B', object.array[1].b);
  assertEquals('C', object.array[1].c);
  assertEquals('D', object.array[1].d);
})();
