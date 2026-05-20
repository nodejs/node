// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

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
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
    }
  }

  var s = new A("foo");
  assertTrue(s instanceof Object);
  assertTrue(s instanceof A);
  assertEquals("object", typeof s);
  checkPrototypeChain(s, [A, Object]);
  assertEquals(42, s.a);
  assertEquals(4.2, s.d);
  assertEquals(153, s.o.foo);

  var s1 = new A("bar");
  assertTrue(%HaveSameMap(s, s1));


  var n = new A(153);
  assertTrue(n instanceof Object);
  assertTrue(n instanceof A);
  assertEquals("object", typeof s);
  checkPrototypeChain(s, [A, Object]);
  assertEquals(42, n.a);
  assertEquals(4.2, n.d);
  assertEquals(153, n.o.foo);

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
  assertEquals(153, b.o.foo);

  var b1 = new A(true);
  assertTrue(%HaveSameMap(b, b1));
  assertTrue(%HaveSameMap(b, s));

  gc();
})();


(function() {
  class A extends Function {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
    }
  }
  var sloppy_func = new A("");
  var strict_func = new A("'use strict';");
  assertNull(sloppy_func.caller);
  assertThrows("strict_f.caller");
  assertEquals(undefined, Object.getOwnPropertyDescriptor(strict_func, "caller"));

  function CheckFunction(func) {
    assertEquals("function", typeof func);
    assertTrue(func instanceof Object);
    assertTrue(func instanceof Function);
    assertTrue(func instanceof A);
    checkPrototypeChain(func, [A, Function, Object]);
    assertEquals(42, func.a);
    assertEquals(4.2, func.d);
    assertEquals(153, func.o.foo);
    assertTrue(undefined !== func.prototype);
    func.prototype.bar = "func.bar";
    var obj = new func();
    assertTrue(obj instanceof Object);
    assertTrue(obj instanceof func);
    assertEquals("object", typeof obj);
    assertEquals(113, obj.foo);
    assertEquals("func.bar", obj.bar);
    delete func.prototype.bar;
  }

  var source = "this.foo = 113;";

  // Sloppy function
  var sloppy_func = new A(source);
  assertTrue(undefined !== sloppy_func.prototype);
  CheckFunction(sloppy_func, false);

  var sloppy_func1 = new A("return 312;");
  assertTrue(%HaveSameMap(sloppy_func, sloppy_func1));

  // Strict function
  var strict_func = new A("'use strict'; " + source);
  assertFalse(%HaveSameMap(strict_func, sloppy_func));
  CheckFunction(strict_func, false);

  var strict_func1 = new A("'use strict'; return 312;");
  assertTrue(%HaveSameMap(strict_func, strict_func1));

  gc();
})();


(function() {
  class A extends Boolean {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A(false);
  assertTrue(%HaveSameMap(o, o1));

  gc();
})();


function TestErrorSubclassing(error) {
  class A extends error {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A("achtung!");
  assertTrue(%HaveSameMap(o, o1));

  gc();
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
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A(312);
  assertTrue(%HaveSameMap(o, o1));

  gc();
})();


(function() {
  class A extends Date {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A(2015, 10, 29);
  assertEquals(2015, o1.getFullYear());
  assertEquals(10, o1.getMonth());
  assertEquals(29, o1.getDate());
  assertTrue(%HaveSameMap(o, o1));

  gc();
})();


(function() {
  class A extends String {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A("bar");
  assertTrue(%HaveSameMap(o, o1));

  gc();
})();


(function() {
  class A extends RegExp {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A(7);
  assertTrue(%HaveSameMap(o, o1));

  gc();
})();


(function TestArraySubclassing() {
  class A extends Array {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
    }
  }

  var o = new Array(13);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Array);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [Array, Object]);
  assertEquals(13, o.length);

  var o = new A(10);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Array);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, Array, Object]);
  assertEquals(10, o.length);
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);
  assertEquals(153, o.o.foo);

  var o1 = new A(7);
  assertTrue(%HaveSameMap(o, o1));
})();


var TypedArray = Uint8Array.__proto__;

function TestTypedArraySubclassing(array) {
  class A extends array {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
    }
  }

  var o = new array(13);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof TypedArray);
  assertTrue(o instanceof array);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [array, TypedArray, Object]);
  assertEquals(13, o.length);

  var o = new A(10);
  assertTrue(o instanceof Object);
  assertTrue(o instanceof TypedArray);
  assertTrue(o instanceof array);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, array, TypedArray, Object]);
  assertEquals(10, o.length);
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);
  assertEquals(153, o.o.foo);

  var o1 = new A(7);
  assertTrue(%HaveSameMap(o, o1));
}


(function() {
  TestTypedArraySubclassing(Int8Array);
  TestTypedArraySubclassing(Uint8Array);
  TestTypedArraySubclassing(Uint8ClampedArray);
  TestTypedArraySubclassing(Int16Array);
  TestTypedArraySubclassing(Uint16Array);
  TestTypedArraySubclassing(Int32Array);
  TestTypedArraySubclassing(Uint32Array);
  TestTypedArraySubclassing(Float32Array);
  TestTypedArraySubclassing(Float64Array);
})();


function TestMapSetSubclassing(container, is_map) {
  var keys = [{name: "banana"}, {name: "cow"}, {name: "orange"}, {name: "chicken"}, {name: "apple"}];

  class A extends container {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A();
  assertTrue(%HaveSameMap(o, o1));

  gc();
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
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

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

  gc();
})();


(function() {
  class A extends DataView {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
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
  assertEquals(153, o.o.foo);

  var o1 = new A(buffer);
  assertTrue(%HaveSameMap(o, o1));

  gc();
})();


(function() {
  var GeneratorFunction = (function*() {}).constructor;
  class A extends GeneratorFunction {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
    }
  }
  var sloppy_func = new A("yield 153;");
  var strict_func = new A("'use strict'; yield 153;");
  // Unfortunately the difference is not observable from outside.
  assertThrows("sloppy_func.caller");
  assertThrows("strict_f.caller");
  assertEquals(undefined, Object.getOwnPropertyDescriptor(sloppy_func, "caller"));
  assertEquals(undefined, Object.getOwnPropertyDescriptor(strict_func, "caller"));

  function CheckFunction(func) {
    assertEquals("function", typeof func);
    assertTrue(func instanceof Object);
    assertTrue(func instanceof Function);
    assertTrue(func instanceof GeneratorFunction);
    assertTrue(func instanceof A);
    checkPrototypeChain(func, [A, GeneratorFunction, Function, Object]);

    assertEquals(42, func.a);
    assertEquals(4.2, func.d);
    assertEquals(153, func.o.foo);

    assertTrue(undefined !== func.prototype);
    func.prototype.bar = "func.bar";
    var obj = func();  // Generator object.
    assertTrue(obj instanceof Object);
    assertTrue(obj instanceof func);
    assertEquals("object", typeof obj);
    assertEquals("func.bar", obj.bar);
    delete func.prototype.bar;

    assertPropertiesEqual({done: false, value: 1}, obj.next());
    assertPropertiesEqual({done: false, value: 1}, obj.next());
    assertPropertiesEqual({done: false, value: 2}, obj.next());
    assertPropertiesEqual({done: false, value: 3}, obj.next());
    assertPropertiesEqual({done: false, value: 5}, obj.next());
    assertPropertiesEqual({done: false, value: 8}, obj.next());
    assertPropertiesEqual({done: true, value: undefined}, obj.next());
  }

  var source = "yield 1; yield 1; yield 2; yield 3; yield 5; yield 8;";

  // Sloppy generator function
  var sloppy_func = new A(source);
  assertTrue(undefined !== sloppy_func.prototype);
  CheckFunction(sloppy_func, false);

  var sloppy_func1 = new A("yield 312;");
  assertTrue(%HaveSameMap(sloppy_func, sloppy_func1));

  // Strict generator function
  var strict_func = new A("'use strict'; " + source);
  assertFalse(%HaveSameMap(strict_func, sloppy_func));
  CheckFunction(strict_func, false);

  var strict_func1 = new A("'use strict'; yield 312;");
  assertTrue(%HaveSameMap(strict_func, strict_func1));

  gc();
})();


(function() {
  class A extends Promise {
    constructor(...args) {
      assertFalse(new.target === undefined);
      super(...args);
      this.a = 42;
      this.d = 4.2;
      this.o = {foo:153};
    }
  }

  var o = new A(function(resolve, reject) {
    resolve("ok");
  });
  assertTrue(o instanceof Object);
  assertTrue(o instanceof Promise);
  assertTrue(o instanceof A);
  assertEquals("object", typeof o);
  checkPrototypeChain(o, [A, Promise, Object]);
  assertEquals(42, o.a);
  assertEquals(4.2, o.d);
  assertEquals(153, o.o.foo);
  o.then(
      function(val) { assertEquals("ok", val); },
      function(reason) { assertUnreachable(); })
    .catch(function(reason) { %AbortJS("catch handler called: " + reason); });

  var o1 = new A(function(resolve, reject) {
    reject("fail");
  });
  o1.then(
      function(val) { assertUnreachable(); },
      function(reason) { assertEquals("fail", reason); })
    .catch(function(reason) { %AbortJS("catch handler called: " + reason); });
  assertTrue(%HaveSameMap(o, o1));

  gc();
})();


(function() {
  class A extends Boolean {
    constructor() {
      assertFalse(new.target === undefined);
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
      assertFalse(new.target === undefined);
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
      assertFalse(new.target === undefined);
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

  gc();
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


(function() {
  function f() {}

  var p = f.prototype;
  var p2 = {};
  var o = Reflect.construct(
        Number, [{valueOf() { f.prototype=p2; return 10; }}], f);

  assertTrue(o.__proto__ === f.prototype);
  assertTrue(p2 === f.prototype);
  assertFalse(p === o.__proto__);
  assertEquals(10, Number.prototype.valueOf.call(o));
})();


(function() {
  function f() {}

  var p = f.prototype;
  var p2 = {};
  var o = Reflect.construct(
        String, [{toString() { f.prototype=p2; return "biep"; }}], f);

  assertTrue(o.__proto__ === f.prototype);
  assertTrue(p2 === o.__proto__);
  assertFalse(p === o.__proto__);
  assertEquals("biep", String.prototype.toString.call(o));
})();


(function() {
  function f() {}

  var p = f.prototype;
  var p2 = {};
  var o = Reflect.construct(
        Date, [{valueOf() { f.prototype=p2; return 1447836899614; }}], f);

  assertTrue(o.__proto__ === f.prototype);
  assertTrue(p2 === f.prototype);
  assertFalse(p === o.__proto__);
  assertEquals(new Date(1447836899614).toString(),
               Date.prototype.toString.call(o));
})();


(function() {
  function f() {}

  var p = f.prototype;
  var p2 = {};
  var o = Reflect.construct(
        Date, [2015, {valueOf() { f.prototype=p2; return 10; }}], f);

  assertTrue(o.__proto__ === f.prototype);
  assertTrue(p2 === f.prototype);
  assertFalse(p === o.__proto__);
  assertEquals(new Date(2015, 10).getYear(), Date.prototype.getYear.call(o));
  assertEquals(new Date(2015, 10).getMonth(), Date.prototype.getMonth.call(o));
})();


(function() {
  function f() {}

  var p = f.prototype;
  var p2 = {};
  var o = Reflect.construct(
        DataView, [new ArrayBuffer(100),
                   {valueOf(){ f.prototype=p2; return 5; }}], f);

  var byteOffset = Object.getOwnPropertyDescriptor(
      DataView.prototype, "byteOffset").get;
  var byteLength = Object.getOwnPropertyDescriptor(
      DataView.prototype, "byteLength").get;

  assertTrue(o.__proto__ === f.prototype);
  assertTrue(p2 === f.prototype);
  assertFalse(p === o.__proto__);
  assertEquals(5, byteOffset.call(o));
  assertEquals(95, byteLength.call(o));
})();


(function() {
  function f() {}

  var p = f.prototype;
  var p2 = {};
  var o = Reflect.construct(
        DataView, [new ArrayBuffer(100),
                   30, {valueOf() { f.prototype=p2; return 5; }}], f);

  var byteOffset = Object.getOwnPropertyDescriptor(
      DataView.prototype, "byteOffset").get;
  var byteLength = Object.getOwnPropertyDescriptor(
      DataView.prototype, "byteLength").get;

  assertTrue(o.__proto__ === f.prototype);
  assertTrue(p2 === f.prototype);
  assertFalse(p === o.__proto__);
  assertEquals(30, byteOffset.call(o));
  assertEquals(5, byteLength.call(o));
})();


(function() {
  function f() {}

  var p = f.prototype;
  var p2 = {};
  var p3 = {};

  var log = [];

  var pattern = {toString() {
    log.push("tostring");
    f.prototype = p3; return "biep" }};

  Object.defineProperty(pattern, Symbol.match, {
    get() { log.push("match"); f.prototype = p2; return false; }});

  var o = Reflect.construct(RegExp, [pattern], f);
  assertEquals(["match", "tostring"], log);
  // TODO(littledan): Is the RegExp constructor correct to create
  // the internal slots and do these type checks this way?
  assertThrows(() => Object.getOwnPropertyDescriptor(RegExp.prototype,
                                                     'source').get(o),
               TypeError);
  assertEquals("/undefined/undefined", RegExp.prototype.toString.call(o));
  assertTrue(o.__proto__ === p2);
  assertTrue(f.prototype === p3);
})();
