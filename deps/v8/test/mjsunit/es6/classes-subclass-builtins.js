// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";


function checkPrototypeChain(object, constructors) {
  var proto = object.__proto__;
  for (var i = 0; i < constructors.length; i++) {
    assertEquals(constructors[i].prototype, proto);
    assertEquals(constructors[i], proto.constructor);
    proto = proto.__proto__;
  }
}


(function() {
  class A extends Object {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var s = new A("foo");
  assertTrue(s instanceof Object);
  assertTrue(s instanceof A);
  assertEquals("object", typeof s);
  checkPrototypeChain(s, [A, Object]);
  assertEquals(42, s.a);
  assertEquals(4.2, s.d);

  var s1 = new A("bar");
  assertTrue(%HaveSameMap(s, s1));


  var n = new A(153);
  assertTrue(n instanceof Object);
  assertTrue(n instanceof A);
  assertEquals("object", typeof s);
  checkPrototypeChain(s, [A, Object]);
  assertEquals(42, n.a);
  assertEquals(4.2, n.d);

  var n1 = new A(312);
  assertTrue(%HaveSameMap(n, n1));
  assertTrue(%HaveSameMap(n, s));


  var b = new A(true);
  assertTrue(b instanceof Object);
  assertTrue(b instanceof A);
  assertEquals("object", typeof s);
  checkPrototypeChain(s, [A, Object]);
  assertEquals(42, b.a);
  assertEquals(4.2, b.d);

  var b1 = new A(true);
  assertTrue(%HaveSameMap(b, b1));
  assertTrue(%HaveSameMap(b, s));
})();


(function() {
  class A extends Function {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A("this.foo = 153;");
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Function);
  assertTrue(o instanceof A);
  assertEquals("function", typeof o);
  checkPrototypeChain(o, [A, Function, Object]);
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);
  var oo = new o();
  assertEquals(153, oo.foo);

  var o1 = new A("return 312;");
  assertTrue(%HaveSameMap(o, o1));
})();


(function() {
  class A extends Boolean {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A(true);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Boolean);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, Boolean]);
  assertTrue(o.valueOf());
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A(false);
  assertTrue(%HaveSameMap(o, o1));
})();


function TestErrorSubclassing(error) {
  class A extends error {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A("message");
  assertTrue(o instanceof Object);
  assertTrue(o instanceof error);
  assertTrue(o instanceof Error);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  if (error == Error) {
    checkPrototypeChain(o, [A, Error, Object]);
  } else {
    checkPrototypeChain(o, [A, error, Error, Object]);
  }
  assertEquals("message", o.message);
  assertEquals(error.name + ": message", o.toString());
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A("achtung!");
  assertTrue(%HaveSameMap(o, o1));
}


(function() {
  TestErrorSubclassing(Error);
  TestErrorSubclassing(EvalError);
  TestErrorSubclassing(RangeError);
  TestErrorSubclassing(ReferenceError);
  TestErrorSubclassing(SyntaxError);
  TestErrorSubclassing(TypeError);
  TestErrorSubclassing(URIError);
})();


(function() {
  class A extends Number {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A(153);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Number);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, Number, Object]);
  assertEquals(153, o.valueOf());
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A(312);
  assertTrue(%HaveSameMap(o, o1));
})();


(function() {
  class A extends Date {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A(1234567890);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Date);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, Date, Object]);
  assertEquals(1234567890, o.getTime());
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A(2015, 10, 29);
  assertEquals(2015, o1.getFullYear());
  assertEquals(10, o1.getMonth());
  assertEquals(29, o1.getDate());
  assertTrue(%HaveSameMap(o, o1));
})();


(function() {
  class A extends String {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A("foo");
  assertTrue(o instanceof Object);
  assertTrue(o instanceof String);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, String, Object]);

  assertEquals("foo", o.valueOf());
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A("bar");
  assertTrue(%HaveSameMap(o, o1));
})();


(function() {
  class A extends RegExp {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A("o(..)h", "g");
  assertTrue(o instanceof Object);
  assertTrue(o instanceof RegExp);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, RegExp, Object]);
  assertTrue(o.test("ouch"));
  assertArrayEquals(["ouch", "uc"], o.exec("boom! ouch! bam!"));
  assertEquals("o(..)h", o.source);
  assertTrue(o.global);
  assertFalse(o.ignoreCase);
  assertFalse(o.multiline);
  assertEquals(10, o.lastIndex);
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A(7);
  assertTrue(%HaveSameMap(o, o1));
})();


function TestArraySubclassing(array) {
  class A extends array {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new array(13);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof array);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [array, Object]);
  assertEquals(13, o.length);

  var o = new A(10);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof array);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, array, Object]);
  assertEquals(10, o.length);
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A(7);
  assertTrue(%HaveSameMap(o, o1));
}


(function() {
  TestArraySubclassing(Array);
  TestArraySubclassing(Int8Array);
  TestArraySubclassing(Uint8Array);
  TestArraySubclassing(Uint8ClampedArray);
  TestArraySubclassing(Int16Array);
  TestArraySubclassing(Uint16Array);
  TestArraySubclassing(Int32Array);
  TestArraySubclassing(Uint32Array);
  TestArraySubclassing(Float32Array);
  TestArraySubclassing(Float64Array);
})();


function TestMapSetSubclassing(container, is_map) {
  var keys = [{name: "banana"}, {name: "cow"}, {name: "orange"}, {name: "chicken"}, {name: "apple"}];

  class A extends container {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A();
  assertTrue(o instanceof Object);
  assertTrue(o instanceof container);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, container, Object]);

  for (var i = 0; i < keys.length; i++) {
    if (is_map) {
      o.set(keys[i], (i + 1) * 11);
    } else {
      o.add(keys[i]);
    }
  }
  o.delete(keys[1]);
  o.delete(keys[3]);

  assertTrue(o.has(keys[0]));
  assertFalse(o.has(keys[1]));
  assertTrue(o.has(keys[2]));
  assertFalse(o.has(keys[1]));
  assertTrue(o.has(keys[4]));
  if (is_map) {
    assertEquals(11, o.get(keys[0]));
    assertEquals(undefined, o.get(keys[1]));
    assertEquals(33, o.get(keys[2]));
    assertEquals(undefined, o.get(keys[3]));
    assertEquals(55, o.get(keys[4]));
  }
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A();
  assertTrue(%HaveSameMap(o, o1));
}


(function() {
  TestMapSetSubclassing(Map, true);
  TestMapSetSubclassing(WeakMap, true);
  TestMapSetSubclassing(Set, false);
  TestMapSetSubclassing(WeakSet, false);
})();


(function() {
  class A extends ArrayBuffer {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var o = new A(16);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof ArrayBuffer);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, ArrayBuffer, Object]);

  assertEquals(16, o.byteLength);
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A("bar");
  assertTrue(%HaveSameMap(o, o1));


  class MyInt32Array extends Int32Array {
    constructor(v, name) {
      super(v);
      this.name = name;
    }
  }

  class MyUint32Array extends Uint32Array {
    constructor(v, name) {
      super(v);
      this.name = name;
    }
  }

  var int32view = new MyInt32Array(o, "cats");
  var uint32view = new MyUint32Array(o, "dogs");

  int32view[0] = -2;
  uint32view[1] = 0xffffffff;

  assertEquals("cats", int32view.name);
  assertEquals("dogs", uint32view.name);
  assertEquals(-2, int32view[0]);
  assertEquals(-1, int32view[1]);
  assertEquals(0xfffffffe, uint32view[0]);
  assertEquals(0xffffffff, uint32view[1]);
})();


(function() {
  class A extends DataView {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }

  var buffer = new ArrayBuffer(16);
  var o = new A(buffer);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof DataView);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, DataView, Object]);

  o.setUint32(0, 0xcafebabe, false);
  assertEquals(0xcafebabe, o.getUint32(0, false));
  assertEquals(0xbebafeca, o.getUint32(0, true));
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);

  var o1 = new A(buffer);
  assertTrue(%HaveSameMap(o, o1));

})();


(function() {
  // TODO(ishell): remove once GeneratorFunction is available.
  var GeneratorFunction = (function*() {}).__proto__.constructor;
  class A extends GeneratorFunction {
    constructor(...args) {
      assertTrue(%IsConstructCall());
      super(...args);
      this.a = 42;
      this.d = 4.2;
    }
  }
  var generator_func = new A("var index = 0; while (index < 5) { yield ++index; }");
  assertTrue(generator_func instanceof Object);
  assertTrue(generator_func instanceof Function);
  assertTrue(generator_func instanceof GeneratorFunction);
  assertTrue(generator_func instanceof A);
  assertEquals("function", typeof generator_func);
  checkPrototypeChain(generator_func, [A, GeneratorFunction, Function, Object]);
  assertEquals(42, generator_func.a);
  assertEquals(4.2, generator_func.d);

  var o = new generator_func();
  assertTrue(o instanceof Object);
  assertTrue(o instanceof generator_func);
  assertEquals("object", typeof o);

  assertPropertiesEqual({done: false, value: 1}, o.next());
  assertPropertiesEqual({done: false, value: 2}, o.next());
  assertPropertiesEqual({done: false, value: 3}, o.next());
  assertPropertiesEqual({done: false, value: 4}, o.next());
  assertPropertiesEqual({done: false, value: 5}, o.next());
  assertPropertiesEqual({done: true, value: undefined}, o.next());

  var generator_func1 = new A("return 0;");
  assertTrue(%HaveSameMap(generator_func, generator_func1));
})();


(function() {
  class A extends Boolean {
    constructor() {
      assertTrue(%IsConstructCall());
      super(true);
      this.a00 = 0
      this.a01 = 0
      this.a02 = 0
      this.a03 = 0
      this.a04 = 0
      this.a05 = 0
      this.a06 = 0
      this.a07 = 0
      this.a08 = 0
      this.a09 = 0
      this.a10 = 0
      this.a11 = 0
      this.a12 = 0
      this.a13 = 0
      this.a14 = 0
      this.a15 = 0
      this.a16 = 0
      this.a17 = 0
      this.a18 = 0
      this.a19 = 0
    }
  }

  class B extends A {
    constructor() {
      assertTrue(%IsConstructCall());
      super();
      this.b00 = 0
      this.b01 = 0
      this.b02 = 0
      this.b03 = 0
      this.b04 = 0
      this.b05 = 0
      this.b06 = 0
      this.b07 = 0
      this.b08 = 0
      this.b09 = 0
      this.b10 = 0
      this.b11 = 0
      this.b12 = 0
      this.b13 = 0
      this.b14 = 0
      this.b15 = 0
      this.b16 = 0
      this.b17 = 0
      this.b18 = 0
      this.b19 = 0
    }
  }

  class C extends B {
    constructor() {
      assertTrue(%IsConstructCall());
      super();
      this.c00 = 0
      this.c01 = 0
      this.c02 = 0
      this.c03 = 0
      this.c04 = 0
      this.c05 = 0
      this.c06 = 0
      this.c07 = 0
      this.c08 = 0
      this.c09 = 0
      this.c10 = 0
      this.c11 = 0
      this.c12 = 0
      this.c13 = 0
      this.c14 = 0
      this.c15 = 0
      this.c16 = 0
      this.c17 = 0
      this.c18 = 0
      this.c19 = 0
    }
  }

  var o = new C();
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Boolean);
  assertTrue(o instanceof A);
  assertTrue(o instanceof B);
  assertTrue(o instanceof C);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [C, B, A, Boolean, Object]);
})();


(function() {
  assertThrows("class A extends undefined {}");
  assertThrows("class B extends NaN {}");
  assertThrows("class C extends Infinity {}");
})();


(function() {
  class A extends null {}
  assertThrows("new A");
})();


(function() {
  class A extends Symbol {}
  assertThrows("new A");
})();
