// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testSpreadCallsStrict() {
  "use strict"
  function countArgs() { return arguments.length; }

  // Test this argument
  function returnThis() { return this; }
  assertEquals(void 0, returnThis(..."test"));

  // Test argument counting with different iterables
  assertEquals(0, countArgs(...""));
  assertEquals(4, countArgs(..."test"));
  assertEquals(4, countArgs(..."tes", ..."t"));
  assertEquals(4, countArgs("t", ..."es", "t"));
  assertEquals(4, countArgs("tes", ..."t!!"));

  assertEquals(1, countArgs(...[1]));
  assertEquals(2, countArgs(...[1, 2]));
  assertEquals(3, countArgs(...[1, 2, 3]));
  assertEquals(4, countArgs(...[1, 2, 3, 4]));
  assertEquals(5, countArgs(...[1, 2, 3, 4, 5]));
  assertEquals(6, countArgs(...[1, 2, 3, 4, 5, 6]));

  assertEquals(1, countArgs(...[1.1]));
  assertEquals(2, countArgs(...[1.1, 2.2]));
  assertEquals(3, countArgs(...[1.1, 2.2, 3.3]));
  assertEquals(4, countArgs(...[1.1, 2.2, 3.3, 4.4]));
  assertEquals(5, countArgs(...[1.1, 2.2, 3.3, 4.4, 5.5]));
  assertEquals(6, countArgs(...[1.1, 2.2, 3.3, 4.4, 5.5, 6.6]));

  assertEquals(1, countArgs(...new Set([1])));
  assertEquals(2, countArgs(...new Set([1, 2])));
  assertEquals(3, countArgs(...new Set([1, 2, 3])));
  assertEquals(4, countArgs(...new Set([1, 2, 3, 4])));
  assertEquals(5, countArgs(...new Set([1, 2, 3, 4, 5])));
  assertEquals(6, countArgs(...new Set([1, 2, 3, 4, 5, 6])));

  assertEquals(3, countArgs(...(function*(){ yield 1; yield 2; yield 3; })()));

  // Test values
  function sum() {
    var sum = arguments[0];
    for (var i = 1; i < arguments.length; ++i) {
      sum += arguments[i];
    }
    return sum;
  }

  assertThrows(function() {
    sum(...0);
  }, TypeError);
  assertEquals(void 0, sum(...""));
  assertEquals(void 0, sum(...[]));
  assertEquals(void 0, sum(...new Set));
  assertEquals(void 0, sum(...(function*() { })()));

  assertEquals("test", sum(..."test"));
  assertEquals(10, sum(...[1, 2, 3, 4]));
  assertEquals(10, sum(...[1, 2, 3], 4));
  assertEquals(10, sum(1, ...[2, 3], 4));
  assertEquals(10, sum(1, ...[2, 3], ...[4]));
  assertEquals(10, sum(1, 2, ...[3, 4]));
  assertEquals(10, sum(...new Set([1, 2, 3, 4])));
  assertEquals(10, sum(...(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
  })()));

  // nested spreads
  function makeArray() {
    var result = [];
    for (var i = 0; i < arguments.length; ++i) {
      result.push(arguments[i]);
    }
    return result;
  }
  assertEquals(10, sum(...makeArray(...[1, 2, 3, 4])));
  assertEquals("test!!!", sum(...makeArray(..."test!!!")));

  // Interleaved spread/unspread args
  assertEquals(36, sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8));
  assertEquals(45, sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8, ...[9]));

  // Methods
  var O = {
    returnThis: returnThis,
    countArgs: countArgs,
    sum: sum,
    makeArray: makeArray,

    nested: {
      returnThis: returnThis,
      countArgs: countArgs,
      sum: sum,
      makeArray: makeArray
    }
  };

  // Test this argument
  assertEquals(O, O.returnThis(..."test"));
  assertEquals(O, O["returnThis"](..."test"));
  assertEquals(O.nested, O.nested.returnThis(..."test"));
  assertEquals(O.nested, O.nested["returnThis"](..."test"));

  // Test argument counting with different iterables
  assertEquals(0, O.countArgs(...""));
  assertEquals(4, O.countArgs(..."test"));
  assertEquals(4, O.countArgs(..."tes", ..."t"));
  assertEquals(4, O.countArgs("t", ..."es", "t"));
  assertEquals(4, O.countArgs("tes", ..."t!!"));

  assertEquals(1, O.countArgs(...[1]));
  assertEquals(2, O.countArgs(...[1, 2]));
  assertEquals(3, O.countArgs(...[1, 2, 3]));
  assertEquals(4, O.countArgs(...[1, 2, 3, 4]));
  assertEquals(5, O.countArgs(...[1, 2, 3, 4, 5]));
  assertEquals(6, O.countArgs(...[1, 2, 3, 4, 5, 6]));

  assertEquals(1, O.countArgs(...new Set([1])));
  assertEquals(2, O.countArgs(...new Set([1, 2])));
  assertEquals(3, O.countArgs(...new Set([1, 2, 3])));
  assertEquals(4, O.countArgs(...new Set([1, 2, 3, 4])));
  assertEquals(5, O.countArgs(...new Set([1, 2, 3, 4, 5])));
  assertEquals(6, O.countArgs(...new Set([1, 2, 3, 4, 5, 6])));

  assertEquals(3, O.countArgs(
      ...(function*(){ yield 1; yield 2; yield 3; })()));

  // Test values
  assertEquals(void 0, O.sum(...""));
  assertEquals(void 0, O.sum(...[]));
  assertEquals(void 0, O.sum(...new Set));
  assertEquals(void 0, O.sum(...(function*() { })()));

  assertEquals("test", O.sum(..."test"));
  assertEquals(10, O.sum(...[1, 2, 3, 4]));
  assertEquals(10, O.sum(...[1, 2, 3], 4));
  assertEquals(10, O.sum(1, ...[2, 3], 4));
  assertEquals(10, O.sum(1, ...[2, 3], ...[4]));
  assertEquals(10, O.sum(1, 2, ...[3, 4]));
  assertEquals(10, O.sum(...new Set([1, 2, 3, 4])));
  assertEquals(10, O.sum(...(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
  })()));

  // nested spreads
  assertEquals(10, O.sum(...O.makeArray(...[1, 2, 3, 4])));
  assertEquals("test!!!", O.sum(...O.makeArray(..."test!!!")));

  // Interleaved spread/unspread args
  assertEquals(36, O.sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8));
  assertEquals(45, O.sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8, ...[9]));
};
testSpreadCallsStrict();
%OptimizeFunctionOnNextCall(testSpreadCallsStrict);
testSpreadCallsStrict();


(function testSpreadCallsSloppy() {
  // Test this argument
  function returnThis() { return this; }
  assertEquals(this, returnThis(..."test"));

  function countArgs() { return arguments.length; }

  // Test argument counting with different iterables
  assertEquals(0, countArgs(...""));
  assertEquals(4, countArgs(..."test"));
  assertEquals(4, countArgs(..."tes", ..."t"));
  assertEquals(4, countArgs("t", ..."es", "t"));
  assertEquals(4, countArgs("tes", ..."t!!"));

  assertEquals(1, countArgs(...[1]));
  assertEquals(2, countArgs(...[1, 2]));
  assertEquals(3, countArgs(...[1, 2, 3]));
  assertEquals(4, countArgs(...[1, 2, 3, 4]));
  assertEquals(5, countArgs(...[1, 2, 3, 4, 5]));
  assertEquals(6, countArgs(...[1, 2, 3, 4, 5, 6]));

  assertEquals(1, countArgs(...new Set([1])));
  assertEquals(2, countArgs(...new Set([1, 2])));
  assertEquals(3, countArgs(...new Set([1, 2, 3])));
  assertEquals(4, countArgs(...new Set([1, 2, 3, 4])));
  assertEquals(5, countArgs(...new Set([1, 2, 3, 4, 5])));
  assertEquals(6, countArgs(...new Set([1, 2, 3, 4, 5, 6])));

  assertEquals(3, countArgs(...(function*(){
    yield 1;
    yield 2;
    yield 3;
  })()));

  // Test values
  function sum() {
    var sum = arguments[0];
    for (var i = 1; i < arguments.length; ++i) {
      sum += arguments[i];
    }
    return sum;
  }

  assertThrows(function() {
    sum(...0);
  }, TypeError);
  assertEquals(void 0, sum(...""));
  assertEquals(void 0, sum(...[]));
  assertEquals(void 0, sum(...new Set));
  assertEquals(void 0, sum(...(function*() { })()));

  assertEquals("test", sum(..."test"));
  assertEquals(10, sum(...[1, 2, 3, 4]));
  assertEquals(10, sum(...[1, 2, 3], 4));
  assertEquals(10, sum(1, ...[2, 3], 4));
  assertEquals(10, sum(1, ...[2, 3], ...[4]));
  assertEquals(10, sum(1, 2, ...[3, 4]));
  assertEquals(10, sum(...new Set([1, 2, 3, 4])));
  assertEquals(10, sum(...(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
  })()));

  // nested spreads
  function makeArray() {
    var result = [];
    for (var i = 0; i < arguments.length; ++i) {
      result.push(arguments[i]);
    }
    return result;
  }
  assertEquals(10, sum(...makeArray(...[1, 2, 3, 4])));
  assertEquals("test!!!", sum(...makeArray(..."test!!!")));

  // Interleaved spread/unspread args
  assertEquals(36, sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8));
  assertEquals(45, sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8, ...[9]));

  // Methods
  var O = {
    returnThis: returnThis,
    countArgs: countArgs,
    sum: sum,
    makeArray: makeArray,

    nested: {
      returnThis: returnThis,
      countArgs: countArgs,
      sum: sum,
      makeArray: makeArray
    }
  };

  // Test this argument
  assertEquals(O, O.returnThis(..."test"));
  assertEquals(O, O["returnThis"](..."test"));
  assertEquals(O.nested, O.nested.returnThis(..."test"));
  assertEquals(O.nested, O.nested["returnThis"](..."test"));

  // Test argument counting with different iterables
  assertEquals(0, O.countArgs(...""));
  assertEquals(4, O.countArgs(..."test"));
  assertEquals(4, O.countArgs(..."tes", ..."t"));
  assertEquals(4, O.countArgs("t", ..."es", "t"));
  assertEquals(4, O.countArgs("tes", ..."t!!"));

  assertEquals(1, O.countArgs(...[1]));
  assertEquals(2, O.countArgs(...[1, 2]));
  assertEquals(3, O.countArgs(...[1, 2, 3]));
  assertEquals(4, O.countArgs(...[1, 2, 3, 4]));
  assertEquals(5, O.countArgs(...[1, 2, 3, 4, 5]));
  assertEquals(6, O.countArgs(...[1, 2, 3, 4, 5, 6]));

  assertEquals(1, O.countArgs(...new Set([1])));
  assertEquals(2, O.countArgs(...new Set([1, 2])));
  assertEquals(3, O.countArgs(...new Set([1, 2, 3])));
  assertEquals(4, O.countArgs(...new Set([1, 2, 3, 4])));
  assertEquals(5, O.countArgs(...new Set([1, 2, 3, 4, 5])));
  assertEquals(6, O.countArgs(...new Set([1, 2, 3, 4, 5, 6])));

  assertEquals(3, O.countArgs(...(function*(){
    yield 1;
    yield 2;
    yield 3;
  })()));

  // Test values
  assertEquals(void 0, O.sum(...""));
  assertEquals(void 0, O.sum(...[]));
  assertEquals(void 0, O.sum(...new Set));
  assertEquals(void 0, O.sum(...(function*() { })()));

  assertEquals("test", O.sum(..."test"));
  assertEquals(10, O.sum(...[1, 2, 3, 4]));
  assertEquals(10, O.sum(...[1, 2, 3], 4));
  assertEquals(10, O.sum(1, ...[2, 3], 4));
  assertEquals(10, O.sum(1, ...[2, 3], ...[4]));
  assertEquals(10, O.sum(1, 2, ...[3, 4]));
  assertEquals(10, O.sum(...new Set([1, 2, 3, 4])));
  assertEquals(10, O.sum(...(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
  })()));

  // nested spreads
  assertEquals(10, O.sum(...O.makeArray(...[1, 2, 3, 4])));
  assertEquals("test!!!", O.sum(...O.makeArray(..."test!!!")));

  // Interleaved spread/unspread args
  assertEquals(36, O.sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8));
  assertEquals(45, O.sum(0, ...[1], 2, 3, ...[4, 5], 6, 7, 8, ...[9]));
})();


(function testSpreadEvaluationOrder() {
  "use strict";
  var log = "";
  function* gen() { log += "X"; yield 0; log += "Y"; }
  function a() { log += "A"; }
  function b() { log += "B"; return gen(); }
  function* c() { log += 'C1'; yield 1; log += 'C2'; }
  function d() { log += "D"; }
  function e() { log += "E"; }
  function fn() {
    var args = [];
    for (var i = 0; i < arguments.length; ++i) args.push(arguments[i]);
    return args;
  }

  var result = fn(a(), ...b(), d());
  assertEquals([undefined, 0, undefined], result);
  assertEquals("ABXYD", log);

  log = "";
  result = fn(...b(), d());
  assertEquals([0, undefined], result);
  assertEquals("BXYD", log);

  log = "";
  result = fn(a(), ...b());
  assertEquals([undefined, 0], result);
  assertEquals("ABXY", log);

  log = "";
  result = fn(a(), ...b(), ...c(), d(), e());
  assertEquals([undefined, 0, 1, undefined, undefined], result);
  assertEquals("ABXYC1C2DE", log);

  log = "";
  result = fn(a(), ...b(), ...c(), d(), e(), ...b(), ...c());
  assertEquals([undefined, 0, 1, undefined, undefined, 0, 1], result);
  assertEquals("ABXYC1C2DEBXYC1C2", log);
})();

(function testArrayPrototypeHoleGetterModifiesIteratorPrototypeNext() {
  function sum() {
    var sum = arguments[0];
    for (var i = 1; i < arguments.length; ++i) {
      sum += arguments[i];
    }
    return sum;
  }
  var a = [1, 2];
  a[3] = 4;
  var called = 0;

  // .next method is only accessed during iteration prologue (see
  // https://github.com/tc39/ecma262/pull/988)
  let ArrayIteratorPrototype = Array.prototype[Symbol.iterator]().__proto__;
  let ArrayIteratorPrototypeNextDescriptor =
      Object.getOwnPropertyDescriptor(ArrayIteratorPrototype, 'next');
  Object.defineProperty(Array.prototype, 2, {
    get: function() {
      var ai = a[Symbol.iterator]();
      var original_next = ai.__proto__["next"];
      Object.defineProperty(ai.__proto__, "next", {
        get: function() {
          called++;
          return original_next;
        },
        configurable: true
      });
      return 3;
    },
    configurable: true
  });

  assertEquals(10, sum(...a));
  assertEquals(0, called);

  Object.defineProperty(ArrayIteratorPrototype, 'next',
                        ArrayIteratorPrototypeNextDescriptor);
  Object.defineProperty(Array.prototype, 2, {});
})();

(function testArrayHasOtherPrototype() {
  function countArgs() { return arguments.length; }
  var a = [1, 2, 3];
  var b = {};
  Object.defineProperty(b, Symbol.iterator, {
    value: function*() {
      yield 4;
    },
    configurable: true
  });

  Object.setPrototypeOf(a, b);

  assertEquals(1, countArgs(...a));
})();

(function testArrayIteratorPrototypeGetter() {
  function countArgs() { return arguments.length; }
  var a = [1, 2, 3];
  var ai = a[Symbol.iterator]();
  var called = 0;

  var original_next = ai.__proto__["next"];

  Object.defineProperty(ai.__proto__, "next", {
    get: function() {
      called++;
      return original_next;
    }
  });

  countArgs(...a);

  // .next method is only accessed during iteration prologue (see
  // https://github.com/tc39/ecma262/pull/988)
  assertEquals(1, called);
})();

(function testArrayIteratorPrototypeModified() {
  function countArgs() { return arguments.length; }
  var a = [1,2,3];
  var ai = a[Symbol.iterator]();
  Object.defineProperty(ai.__proto__, "next", {
    value: function() {
      return {value: undefined, done: true};
     },
     configurable: true
  });

  assertEquals(0, countArgs(...a));

})();

(function testCustomArrayPrototypeIterator() {
  var origIterator =
      Object.getOwnPropertyDescriptor(Array.prototype, Symbol.iterator);
  Object.defineProperty(Array.prototype, Symbol.iterator, {
    value: function*() {
      yield 1;
      yield 2;
      yield 3;
    },
    configurable: true
  });
  function returnCountStrict() { 'use strict'; return arguments.length; }
  function returnCountSloppy() { return arguments.length; }

  assertEquals(3, returnCountStrict(...[1]));
  assertEquals(4, returnCountStrict(1, ...[2]));
  assertEquals(5, returnCountStrict(1, ...[2], 3));
  assertEquals(3, returnCountSloppy(...[1]));
  assertEquals(4, returnCountSloppy(1, ...[2]));
  assertEquals(5, returnCountSloppy(1, ...[2], 3));

  Object.defineProperty(Array.prototype, Symbol.iterator, origIterator);
})();

(function testGetPropertyIteratorCalledExactlyOnce() {
  function countArgs() { return arguments.length; }
  var a = [1, 2, 3];
  var called = 0;

  Object.defineProperty(Array.prototype, Symbol.iterator, {
    value: function*() {
      yield 1;
      yield 2;
    },
    configurable: true
  });

  var it = a[Symbol.iterator];
  Object.defineProperty(a, Symbol.iterator, {
    get: function() {
      called++;
      return it;
    }
  });

  countArgs(...a);

  assertEquals(1, called);
})();
