// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function TestBasics() {
  var object = {
    method() {
      return 42;
    }
  };
  assertEquals(42, object.method());
})();


(function TestThis() {
  var object = {
    method() {
      assertEquals(object, this);
    }
  };
  object.method();
})();


(function TestDescriptor() {
  var object = {
    method() {
      return 42;
    }
  };

  var desc = Object.getOwnPropertyDescriptor(object, 'method');
  assertTrue(desc.enumerable);
  assertTrue(desc.configurable);
  assertTrue(desc.writable);
  assertEquals('function', typeof desc.value);

  assertEquals(42, desc.value());
})();


(function TestProto() {
  var object = {
    method() {}
  };

  assertEquals(Function.prototype, Object.getPrototypeOf(object.method));
})();


(function TestNotConstructable() {
  var object = {
    method() {}
  };

  assertThrows(function() {
    new object.method;
  });
})();


(function TestFunctionName() {
  var object = {
    method() {},
    1() {},
    2.0() {}
  };
  var f = object.method;
  assertEquals('method', f.name);
  var g = object[1];
  assertEquals('1', g.name);
  var h = object[2];
  assertEquals('2', h.name);
})();


(function TestNoBinding() {
  var method = 'local';
  var calls = 0;
  var object = {
    method() {
      calls++;
      assertEquals('local', method);
    }
  };
  object.method();
  assertEquals(1, calls);
})();


(function TestNoPrototype() {
  var object = {
    method() {}
  };
  var f = object.method;
  assertFalse(f.hasOwnProperty('prototype'));
  assertEquals(undefined, f.prototype);

  f.prototype = 42;
  assertEquals(42, f.prototype);
})();


(function TestNoRestrictedPropertiesStrict() {
  var obj = {
    method() { "use strict"; }
  };
  assertFalse(obj.method.hasOwnProperty("arguments"));
  assertThrows(function() { return obj.method.arguments; }, TypeError);
  assertThrows(function() { obj.method.arguments = {}; }, TypeError);

  assertFalse(obj.method.hasOwnProperty("caller"));
  assertThrows(function() { return obj.method.caller; }, TypeError);
  assertThrows(function() { obj.method.caller = {}; }, TypeError);
})();


(function TestNoRestrictedPropertiesSloppy() {
  var obj = {
    method() {}
  };
  assertFalse(obj.method.hasOwnProperty("arguments"));
  assertThrows(function() { return obj.method.arguments; }, TypeError);
  assertThrows(function() { obj.method.arguments = {}; }, TypeError);

  assertFalse(obj.method.hasOwnProperty("caller"));
  assertThrows(function() { return obj.method.caller; }, TypeError);
  assertThrows(function() { obj.method.caller = {}; }, TypeError);
})();


(function TestToString() {
  var object = {
    method() { 42; }
  };
  assertEquals('method() { 42; }', object.method.toString());
})();


(function TestOptimized() {
  var object = {
    method() { return 42; }
  };
  %PrepareFunctionForOptimization(object.method);
  assertEquals(42, object.method());
  assertEquals(42, object.method());
  %OptimizeFunctionOnNextCall(object.method);
  assertEquals(42, object.method());
  assertFalse(object.method.hasOwnProperty('prototype'));
})();


///////////////////////////////////////////////////////////////////////////////


var GeneratorFunction = function*() {}.__proto__.constructor;
var GeneratorPrototype = Object.getPrototypeOf(function*() {}).prototype;


function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}


(function TestGeneratorBasics() {
  var object = {
    *method() {
      yield 1;
    }
  };
  var g = object.method();
  assertIteratorResult(1, false, g.next());
  assertIteratorResult(undefined, true, g.next());
})();


(function TestGeneratorThis() {
  var object = {
    *method() {
      yield this;
    }
  };
  var g = object.method();
  assertIteratorResult(object, false, g.next());
  assertIteratorResult(undefined, true, g.next());
})();


(function TestGeneratorSymbolIterator() {
  var object = {
    *method() {}
  };
  var g = object.method();
  assertEquals(g, g[Symbol.iterator]());
})();


(function TestGeneratorDescriptor() {
  var object = {
    *method() {
      yield 1;
    }
  };

  var desc = Object.getOwnPropertyDescriptor(object, 'method');
  assertTrue(desc.enumerable);
  assertTrue(desc.configurable);
  assertTrue(desc.writable);
  assertEquals('function', typeof desc.value);

  var g = desc.value();
  assertIteratorResult(1, false, g.next());
  assertIteratorResult(undefined, true, g.next());
})();


(function TestGeneratorPrototypeDescriptor() {
  var object = {
    *method() {}
  };

  var desc = Object.getOwnPropertyDescriptor(object.method, 'prototype');
  assertFalse(desc.enumerable);
  assertFalse(desc.configurable);
  assertTrue(desc.writable);
  assertEquals(GeneratorPrototype, Object.getPrototypeOf(desc.value));
})();


(function TestGeneratorProto() {
  var object = {
    *method() {}
  };

  assertEquals(GeneratorFunction.prototype,
               Object.getPrototypeOf(object.method));
})();


(function TestGeneratorNotConstructable() {
  var object = {
    *method() {
      yield 1;
    }
  };

  assertThrows(()=>new object.method());
})();


(function TestGeneratorName() {
  var object = {
    *method() {},
    *1() {},
    *2.0() {}
  };
  var f = object.method;
  assertEquals('method', f.name);
  var g = object[1];
  assertEquals('1', g.name);
  var h = object[2];
  assertEquals('2', h.name);
})();


(function TestGeneratorNoBinding() {
  var method = 'local';
  var calls = 0;
  var object = {
    *method() {
      calls++;
      assertEquals('local', method);
    }
  };
  var g = object.method();
  assertIteratorResult(undefined, true, g.next());
  assertEquals(1, calls);
})();


(function TestGeneratorToString() {
  var object = {
    *method() { yield 1; }
  };
  assertEquals('*method() { yield 1; }', object.method.toString());
})();


(function TestProtoName() {
  var object = {
    __proto__() {
      return 1;
    }
  };
  assertEquals(Object.prototype, Object.getPrototypeOf(object));
  assertEquals(1, object.__proto__());
})();


(function TestProtoName2() {
  var p = {};
  var object = {
    __proto__() {
      return 1;
    },
    __proto__: p
  };
  assertEquals(p, Object.getPrototypeOf(object));
  assertEquals(1, object.__proto__());
})();
