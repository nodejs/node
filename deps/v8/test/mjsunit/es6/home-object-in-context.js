// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestSuperInObjectLiteralMethod() {
  let my_proto = {
    __proto__ : {'x': 'right' },
    m() { return super.x; }
  };
  let o = {__proto__: my_proto};
  assertEquals('right', o.m());
})();

(function TestSuperInObjectLiteralGetter() {
  let my_proto = {
    __proto__ : {'x': 'right' },
    get p() { return super.x; }
  };
  let o = {__proto__: my_proto};
  assertEquals('right', o.p);
})();

(function TestSuperInObjectLiteralSetter() {
  let read_value;
  let my_proto = {
    __proto__ : {'x': 'right' },
    set p(x) { read_value = super.x; }
  };
  let o = {__proto__: my_proto};
  o.p = 'whatever';
  assertEquals('right', read_value);
})();

(function TestSuperInObjectLiteralProperty() {
  class OuterBase {};
  OuterBase.prototype.x = 'right';
  class Outer extends OuterBase {
    m2() {
      let my_proto = {
        __proto__ : {'x': 'wrong' },
        m: () => super.x,
      };
      let o = {__proto__: my_proto};
      return o.m();
    }
  }
  assertEquals('right', (new Outer()).m2());
})();

(function TestMethodScopes() {
  let object = { // object literal 1 starts
    __proto__: { // object literal 2 starts
      method1() { return 'right'; }
    }, // object literal 2 ends
    method2() {
      return super.method1();
    }
  }; // object literal 1 ends
  assertEquals('right', object.method2());
})();

(function TestEvalInObjectLiteral() {
  let o = {__proto__: {x: 'right'},
           x: 'wrong',
           m() {
             let r = 0;
             eval('r = super.x;');
             return r;
           }
          };

  assertEquals('right', o.m());
})();

(function TestEvalInMethod() {
  class A {};
  A.prototype.x = 'right';

  class B extends A {
    m() {
      let r;
      eval('r = super.x;');
      return r;
    }
  };
  B.prototype.x = 'wrong';

  let b = new B();
  assertEquals('right', b.m());
})();

(function TestSuperInsidePropertyInitializer() {
  class OuterBase {}
  OuterBase.prototype.prop = 'wrong';
  OuterBase.prop = 'wrong';

  class Outer extends OuterBase {
    m() {
      class A { }
      A.prototype.prop = 'right';

      class B extends A {
        x = () => { return super.prop; };
      }

      B.prototype.prop = 'wrong';
      return (new B()).x();
    }
  }
  Outer.prototype.prop = 'wrong';
  Outer.prop = 'wrong';

  assertEquals('right', (new Outer()).m());
})();

(function TestSuperInsideStaticPropertyInitializer() {
  class OuterBase {}
  OuterBase.prototype.prop = 'wrong';
  OuterBase.prop = 'wrong';

  class Outer extends OuterBase {
    m() {
      class A { }
      A.prop = 'right';
      A.prototype.prop = 'wrong';
      class B extends A {
        static x = super.prop;
      }
      B.prop = 'wrong';
      B.prototype.prop = 'wrong';
      return B.x;
    }
  }
  Outer.prototype.prop = 'wrong';
  Outer.prop = 'wrong';

  assertEquals('right', (new Outer).m());
})();

(function TestSuperInsideStaticPropertyInitializer2() {
  class A extends class {
                      a() { return 'wrong'; }
                  } {
    m() {
      class C extends class {
                        static a() { return 'right'; }
                      } {
        static static_prop = super.a;
      };
      return C.static_prop;
    }
  };
  assertEquals('right', (new A()).m()());
})();

(function TestSuperInsideExtends() {
  class C extends class {
                    static a = 'right';
                  } {
    static m = class D extends new Proxy(function f() {},
                                         {get:(t, k) => {
                                            if (k == "prototype") {
                                              return Function.prototype;
                                            }
                                            return super.a;
                                          }
                                         }) {}
  };
  assertEquals('right', C.m.a);
})();

// Same as the previous test but without a Proxy.
(function TestSuperInsideExtends2() {
  function f(x) {
    function A() { }
    A.x = x;
    return A;
  }

  class B {};
  B.a = 'right';

  // How to write "super" inside the extends clause? The "class extends value"
  // needs to be a constructor.
  class C extends B {
    static m = class D extends f({m2: () => { return super.a;}}) { }
  }

  // C.m is a class. Its "parent class" is a function (returned by f). C.m.x
  // binds to the parent's x, which is whatever we passed as a param to f.
  // In this case, it's an object which has a property m2.

  // Since m2 is an arrow function, and not a method, "super" inside it
  // doesn't bind to the object where m2 is defined, but outside.
  assertEquals('right', C.m.x.m2());
})();
