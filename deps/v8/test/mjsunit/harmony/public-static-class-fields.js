// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-public-fields --harmony-static-fields

"use strict";

{
  class C {
    static a;
  }

  assertEquals(undefined, C.a);
  let descriptor = Object.getOwnPropertyDescriptor(C, 'a');
  assertTrue(C.hasOwnProperty('a'));
  assertTrue(descriptor.writable);
  assertTrue(descriptor.enumerable);
  assertTrue(descriptor.configurable);

  let c = new C;
  assertEquals(undefined, c.a);
}

{
  let x = 'a';
  class C {
    static a;
    static hasOwnProperty = function() { return 1; }
    static b = x;
    static c = 1;
  }

  assertEquals(undefined, C.a);
  assertEquals('a', C.b);
  assertEquals(1, C.c);
  assertEquals(1, C.hasOwnProperty());

  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals(undefined, c.b);
  assertEquals(undefined, c.c);
}

{
  assertThrows(() => {
    class C {
      static x = Object.freeze(this);
      static c = 42;
    }
  }, TypeError);
}

{
  class C {
    static c = this;
    static d = () => this;
  }

  assertEquals(C, C.c);
  assertEquals(C, C.d());

  let c = new C;
  assertEquals(undefined, c.c);
  assertEquals(undefined, c.d);
}

{
  class C {
    static c = 1;
    static d = this.c;
  }

  assertEquals(1, C.c);
  assertEquals(1, C.d);

  let c = new C;
  assertEquals(undefined, c.c);
  assertEquals(undefined, c.d);
}

{
  class C {
    static b = 1;
    static c = () => this.b;
  }

  assertEquals(1, C.b);
  assertEquals(1, C.c());

  let c = new C;
  assertEquals(undefined, c.c);
}

{
  let x = 'a';
  class C {
    static b = 1;
    static c = () => this.b;
    static e = () => x;
  }

  assertEquals(1, C.b);
  assertEquals('a', C.e());

  let a = {b : 2 };
  assertEquals(1, C.c.call(a));

  let c = new C;
  assertEquals(undefined, c.b);
  assertEquals(undefined, c.c);
}

{
  let x = 'a';
  class C {
    static c = 1;
    static d = function() { return this.c; };
    static e = function() { return x; };
  }

  assertEquals(1, C.c);
  assertEquals(1, C.d());
  assertEquals('a', C.e());

  C.c = 2;
  assertEquals(2, C.d());

  let a = {c : 3 };
  assertEquals(3, C.d.call(a));

  assertThrows(C.d.bind(undefined));

  let c = new C;
  assertEquals(undefined, c.c);
  assertEquals(undefined, c.d);
  assertEquals(undefined, c.e);
}

{
  class C {
    static c = function() { return 1 };
  }

  assertEquals('c', C.c.name);
}

{
  let d = function() { return new.target; }
  class C {
    static c = d;
  }

  assertEquals(undefined, C.c());
  assertEquals(new d, new C.c());
}

{
  class C {
    static c = () => new.target;
  }

  assertEquals(undefined, C.c());
}

{
   class C {
     static c = () => {
       let b;
       class A {
         constructor() {
           b = new.target;
         }
       };
       new A;
       assertEquals(A, b);
     }
  }

  C.c();
}

{
  class C {
    static c = new.target;
  }

  assertEquals(undefined, C.c);
}

{
  class B {
    static d = 1;
    static b = () => this.d;
  }

  class C extends B {
    static c = super.d;
    static d = () => super.d;
    static e = () => super.b();
  }

  assertEquals(1, C.c);
  assertEquals(1, C.d());
  assertEquals(1, C.e());
}

{
  let foo = undefined;
  class B {
    static set d(x) {
      foo = x;
    }
  }

  class C extends B {
    static d = 2;
  }

  assertEquals(undefined, foo);
  assertEquals(2, C.d);
}


{
  let C  = class {
    static c;
  };

  assertEquals("C", C.name);
}

{
  class C {
    static c = new C;
  }

  assertTrue(C.c instanceof C);
}

(function test() {
  function makeC() {
    var x = 1;

    return class {
      static a = () => () => x;
    }
  }

  let C = makeC();
  let f = C.a();
  assertEquals(1, f());
})()

{
  let c = "c";
  class C {
    static ["a"] = 1;
    static ["b"];
    static [c];
  }

  assertEquals(1, C.a);
  assertEquals(undefined, C.b);
  assertEquals(undefined, C[c]);
}

{
  let log = [];
  function run(i) {
    log.push(i);
    return i;
  }

  class C {
    static [run(1)] = run(6);
    static [run(2)] = run(7);
    [run(3)]() { run(9);}
    static [run(4)] = run(8);
    static [run(5)]() { throw new Error('should not execute');};
  }

  let c = new C;
  c[3]();
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], log);
}



function x() {

  // This tests lazy parsing.
  return function() {
    let log = [];
    function run(i) {
      log.push(i);
      return i;
    }

    class C {
      static [run(1)] = run(6);
      static [run(2)] = run(7);
      [run(3)]() { run(9);}
      static [run(4)] = run(8);
      static [run(5)]() { throw new Error('should not execute');};
    }

    let c = new C;
    c[3]();
    assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], log);
  }
}
x()();

{
  let log = [];
  function run(i) {
    log.push(i);
    return i;
  }

  class C {
    [run(1)] = run(7);
    [run(2)] = run(8);
    [run(3)]() { run(9);}
    static [run(4)] = run(6);
    [run(5)]() { throw new Error('should not execute');};
  }

  let c = new C;
  c[3]();
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], log);
}

function y() {
  // This tests lazy parsing.
  return function() {
    let log = [];
    function run(i) {
      log.push(i);
      return i;
    }

    class C {
      [run(1)] = run(7);
      [run(2)] = run(8);
      [run(3)]() { run(9);}
      static [run(4)] = run(6);
      [run(5)]() { throw new Error('should not execute');};
    }

    let c = new C;
    c[3]();
    assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], log);
  }
}
y()();

{
  class C {}
  class D {
    static [C];
  }

  assertThrows(() => { class X { static [X] } });
  assertEquals(undefined, D[C]);
}

{
  function t() {
    return class {
      static ['x'] = 2;
    }
  }

  let klass = t();
  let obj = new klass;
  assertEquals(2, klass.x);
}

{
  let x = 'a';
  class C {
    a;
    b = x;
    c = 1;
    hasOwnProperty() { return 1;}
    static [x] = 2;
    static b = 3;
    static d;
  }

  assertEquals(2, C.a);
  assertEquals(3, C.b);
  assertEquals(undefined, C.d);
  assertEquals(undefined, C.c);

  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals('a', c.b);
  assertEquals(1, c.c);
  assertEquals(undefined, c.d);
  assertEquals(1, c.hasOwnProperty());
}

{
  function t() {
    return class {
      ['x'] = 1;
      static ['x'] = 2;
    }
  }

  let klass = t();
  let obj = new klass;
  assertEquals(1, obj.x);
  assertEquals(2, klass.x);
}


{
  class X {
    static p = function() { return arguments[0]; }
  }

  assertEquals(1, X.p(1));
}

{
  class X {
    static t = () => {
      function p() { return arguments[0]; };
      return p;
    }
  }

  let p = X.t();
  assertEquals(1, p(1));
}

{
  class X {
    static t = () => {
      function p() { return eval("arguments[0]"); };
      return p;
    }
  }

  let p = X.t();
  assertEquals(1, p(1));
}

{
  class X {
    static p = eval("(function() { return arguments[0]; })(1)");
  }

  assertEquals(1, X.p);
}

{
  let p = { z: class { static y = this.name } }
  assertEquals(p.z.y, 'z');

  let q = { ["z"]: class { static y = this.name } }
  assertEquals(q.z.y, 'z');

  const C = class {
    static x = this.name;
  }
  assertEquals(C.x, 'C');
}
