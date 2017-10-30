// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';


function ID(x) {
  return x;
}


(function TestClassMethodString() {
  class C {
    a() { return 'A'}
    ['b']() { return 'B'; }
    c() { return 'C'; }
    [ID('d')]() { return 'D'; }
  }
  assertEquals('A', new C().a());
  assertEquals('B', new C().b());
  assertEquals('C', new C().c());
  assertEquals('D', new C().d());
  assertArrayEquals([], Object.keys(C.prototype));
  assertArrayEquals(['constructor', 'a', 'b', 'c', 'd'],
                    Object.getOwnPropertyNames(C.prototype));
})();


(function TestClassMethodNumber() {
  class C {
    a() { return 'A'; }
    [1]() { return 'B'; }
    c() { return 'C'; }
    [ID(2)]() { return 'D'; }
  }
  assertEquals('A', new C().a());
  assertEquals('B', new C()[1]());
  assertEquals('C', new C().c());
  assertEquals('D', new C()[2]());
  // Array indexes first.
  assertArrayEquals([], Object.keys(C.prototype));
  assertArrayEquals(['1', '2', 'constructor', 'a', 'c'],
                    Object.getOwnPropertyNames(C.prototype));
})();


(function TestClassMethodSymbol() {
  var sym1 = Symbol();
  var sym2 = Symbol();
  class C {
    a() { return 'A'; }
    [sym1]() { return 'B'; }
    c() { return 'C'; }
    [ID(sym2)]() { return 'D'; }
  }
  assertEquals('A', new C().a());
  assertEquals('B', new C()[sym1]());
  assertEquals('C', new C().c());
  assertEquals('D', new C()[sym2]());
  assertArrayEquals([], Object.keys(C.prototype));
  assertArrayEquals(['constructor', 'a', 'c'],
                    Object.getOwnPropertyNames(C.prototype));
  assertArrayEquals([sym1, sym2], Object.getOwnPropertySymbols(C.prototype));
})();



(function TestStaticClassMethodString() {
  class C {
    static a() { return 'A'}
    static ['b']() { return 'B'; }
    static c() { return 'C'; }
    static ['d']() { return 'D'; }
  }
  assertEquals('A', C.a());
  assertEquals('B', C.b());
  assertEquals('C', C.c());
  assertEquals('D', C.d());
  assertArrayEquals([], Object.keys(C));
  // TODO(arv): It is not clear that we are adding the "standard" properties
  // in the right order. As far as I can tell the spec adds them in alphabetical
  // order.
  assertArrayEquals(['length', 'prototype', 'a', 'b', 'c', 'd', 'name'],
                    Object.getOwnPropertyNames(C));
})();


(function TestStaticClassMethodNumber() {
  class C {
    static a() { return 'A'; }
    static [1]() { return 'B'; }
    static c() { return 'C'; }
    static [2]() { return 'D'; }
  }
  assertEquals('A', C.a());
  assertEquals('B', C[1]());
  assertEquals('C', C.c());
  assertEquals('D', C[2]());
  // Array indexes first.
  assertArrayEquals([], Object.keys(C));
  assertArrayEquals(['1', '2', 'length', 'prototype', 'a', 'c', 'name'],
                    Object.getOwnPropertyNames(C));
})();


(function TestStaticClassMethodSymbol() {
  var sym1 = Symbol();
  var sym2 = Symbol();
  class C {
    static a() { return 'A'; }
    static [sym1]() { return 'B'; }
    static c() { return 'C'; }
    static [sym2]() { return 'D'; }
  }
  assertEquals('A', C.a());
  assertEquals('B', C[sym1]());
  assertEquals('C', C.c());
  assertEquals('D', C[sym2]());
  assertArrayEquals([], Object.keys(C));
  assertArrayEquals(['length', 'prototype', 'a', 'c', 'name'],
                    Object.getOwnPropertyNames(C));
  assertArrayEquals([sym1, sym2], Object.getOwnPropertySymbols(C));
})();



function assertIteratorResult(value, done, result) {
  assertEquals({ value: value, done: done}, result);
}


(function TestGeneratorComputedName() {
  class C {
    *['a']() {
      yield 1;
      yield 2;
    }
  }
  var iter = new C().a();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertArrayEquals([], Object.keys(C.prototype));
  assertArrayEquals(['constructor', 'a'],
                    Object.getOwnPropertyNames(C.prototype));
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
  class C {
    a() { return 'A'; }
    [key1]() { return 'B'; }
    c() { return 'C'; }
    [key2]() { return 'D'; }
  }
  assertEquals(2, counter);
  assertEquals('A', new C().a());
  assertEquals('B', new C().b());
  assertEquals('C', new C().c());
  assertEquals('D', new C().d());
  assertArrayEquals([], Object.keys(C.prototype));
  assertArrayEquals(['constructor', 'a', 'b', 'c', 'd'],
                    Object.getOwnPropertyNames(C.prototype));
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

  class C {
    a() { return 'A'; }
    [key1]() { return 'B'; }
    c() { return 'C'; }
    [key2]() { return 'D'; }
  }
  assertEquals(2, counter);
  assertEquals('A', new C().a());
  assertEquals('B', new C()[1]());
  assertEquals('C', new C().c());
  assertEquals('D', new C()[2]());
  // Array indexes first.
  assertArrayEquals([], Object.keys(C.prototype));
  assertArrayEquals(['1', '2', 'constructor', 'a', 'c'],
                    Object.getOwnPropertyNames(C.prototype));
})();


(function TestGetter() {
  class C {
    get ['a']() {
      return 'A';
    }
  }
  assertEquals('A', new C().a);

  class C2 {
    get b() {
      assertUnreachable();
    }
    get ['b']() {
      return 'B';
    }
  }
  assertEquals('B', new C2().b);

  class C3 {
    get c() {
      assertUnreachable();
    }
    get ['c']() {
      assertUnreachable();
    }
    get ['c']() {
      return 'C';
    }
  }
  assertEquals('C', new C3().c);

  class C4 {
    get ['d']() {
      assertUnreachable();
    }
    get d() {
      return 'D';
    }
  }
  assertEquals('D', new C4().d);
})();


(function TestSetter() {
  var calls = 0;
  class C {
    set ['a'](_) {
      calls++;
    }
  }
  new C().a = 'A';
  assertEquals(1, calls);

  calls = 0;
  class C2 {
    set b(_) {
      assertUnreachable();
    }
    set ['b'](_) {
      calls++;
    }
  }
  new C2().b = 'B';
  assertEquals(1, calls);

  calls = 0;
  class C3 {
    set c(_) {
      assertUnreachable()
    }
    set ['c'](_) {
      assertUnreachable()
    }
    set ['c'](_) {
      calls++
    }
  }
  new C3().c = 'C';
  assertEquals(1, calls);

  calls = 0;
  class C4 {
    set ['d'](_) {
      assertUnreachable()
    }
    set d(_) {
      calls++
    }
  }
  new C4().d = 'D';
  assertEquals(1, calls);
})();


(function TestPrototype() {
  assertThrows(function() {
    class C {
      static ['prototype']() {
        return 1;
      }
    }
  }, TypeError);

  assertThrows(function() {
    class C2 {
      static get ['prototype']() {
        return 2;
      }
    }
  }, TypeError);

  assertThrows(function() {
    class C3 {
      static set ['prototype'](x) {
        assertEquals(3, x);
      }
    }
  }, TypeError);

  assertThrows(function() {
    class C4 {
      static *['prototype']() {
        yield 1;
        yield 2;
      }
    }
  }, TypeError);
})();


(function TestPrototypeConcat() {
  assertThrows(function() {
    class C {
      static ['pro' + 'tot' + 'ype']() {
        return 1;
      }
    }
  }, TypeError);

  assertThrows(function() {
    class C2 {
      static get ['pro' + 'tot' + 'ype']() {
        return 2;
      }
    }
  }, TypeError);

  assertThrows(function() {
    class C3 {
      static set ['pro' + 'tot' + 'ype'](x) {
        assertEquals(3, x);
      }
    }
  }, TypeError);

  assertThrows(function() {
    class C4 {
      static *['pro' + 'tot' + 'ype']() {
        yield 1;
        yield 2;
      }
    }
  }, TypeError);
})();


(function TestConstructor() {
  // Normally a constructor property is not allowed.
  class C {
    ['constructor']() {
      return 1;
    }
  }
  assertTrue(C !== C.prototype.constructor);
  assertEquals(1, new C().constructor());

  class C2 {
    get ['constructor']() {
      return 2;
    }
  }
  assertEquals(2, new C2().constructor);

  var calls = 0;
  class C3 {
    set ['constructor'](x) {
      assertEquals(3, x);
      calls++;
    }
  }
  new C3().constructor = 3;
  assertEquals(1, calls);

  class C4 {
    *['constructor']() {
      yield 1;
      yield 2;
    }
  }
  var iter = new C4().constructor();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
})();


(function TestExceptionInName() {
  function MyError() {};
  function throwMyError() {
    throw new MyError();
  }
  assertThrows(function() {
    class C {
      [throwMyError()]() {}
    }
  }, MyError);
  assertThrows(function() {
    class C {
      get [throwMyError()]() { return 42; }
    }
  }, MyError);
  assertThrows(function() {
    class C {
      set [throwMyError()](_) { }
    }
  }, MyError);
})();


(function TestTdzName() {
  assertThrows(function() {
    class C {
      [C]() {}
    }
  }, ReferenceError);
  assertThrows(function() {
    class C {
      get [C]() { return 42; }
    }
  }, ReferenceError);
  assertThrows(function() {
    class C {
      set [C](_) { }
    }
  }, ReferenceError);
})();
