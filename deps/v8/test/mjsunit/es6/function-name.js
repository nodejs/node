// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testVariableDeclarationsFunction() {
  'use strict';
  var a = function(){};
  assertEquals('a', a.name);
  let b = () => {};
  assertEquals('b', b.name);
  const c = ((function(){}));
  assertEquals('c', c.name);

  var x = function(){}, y = () => {}, z = function withName() {};
  assertEquals('x', x.name);
  assertEquals('y', y.name);
  assertEquals('withName', z.name);
})();

(function testVariableDeclarationsClass() {
  'use strict';
  var a = class {};
  assertEquals('a', a.name);
  let b = ((class {}));
  assertEquals('b', b.name);
  // Should not overwrite name property.
  const c = class { static name() { } }
  assertEquals('function', typeof c.name);

  var x = class {}, y = class NamedClass {};
  assertEquals('x', x.name);
  assertEquals('NamedClass', y.name);
})();

(function testObjectProperties() {
  'use strict';
  var obj = {
    a: function() {},
    b: () => {},
    c() { },
    get d() { },
    set d(val) { },
    x: function withName() { },
    y: class { },
    z: class ClassName { },
    ''() {},
    42: function() {},
    4.2: function() {},
    __proto__: function() {},
  };

  assertEquals('a', obj.a.name);
  assertEquals('b', obj.b.name);
  assertEquals('c', obj.c.name);
  var dDescriptor = Object.getOwnPropertyDescriptor(obj, 'd');
  assertEquals('get d', dDescriptor.get.name);
  assertEquals('set d', dDescriptor.set.name);
  assertEquals('withName', obj.x.name);
  assertEquals('y', obj.y.name);
  assertEquals('ClassName', obj.z.name);
  assertEquals('', obj[''].name);
  assertEquals('42', obj[42].name);
  assertEquals('4.2', obj[4.2].name);
  assertEquals('', obj.__proto__.name);
})();

(function testClassProperties() {
  'use strict';
  class C {
    a() { }
    static b() { }
    get c() { }
    set c(val) { }
    ''() { }
    static ''() { }
    42() { }
    static 43() { }
    get 44() { }
    set 44(val) { }
    static get constructor() { }
    static set constructor(val) { }
  };

  assertEquals('a', C.prototype.a.name);
  assertEquals('b', C.b.name);
  var descriptor = Object.getOwnPropertyDescriptor(C.prototype, 'c');
  assertEquals('get c', descriptor.get.name);
  assertEquals('set c', descriptor.set.name);
  assertEquals('', C.prototype[''].name);
  assertEquals('', C[''].name);
  assertEquals('42', C.prototype[42].name);
  assertEquals('43', C[43].name);
  var descriptor = Object.getOwnPropertyDescriptor(C.prototype, '44');
  assertEquals('get 44', descriptor.get.name);
  assertEquals('set 44', descriptor.set.name);
  var descriptor = Object.getOwnPropertyDescriptor(C, 'constructor');
  assertEquals('get constructor', descriptor.get.name);
  assertEquals('set constructor', descriptor.set.name);
})();

(function testComputedProperties() {
  'use strict';
  var a = 'a';
  var b = 'b';
  var sym1 = Symbol('1');
  var sym2 = Symbol('2');
  var sym3 = Symbol('3');
  var symNoDescription = Symbol();
  var proto = "__proto__";
  var obj = {
    ['']: function() {},
    [a]: function() {},
    [sym1]: function() {},
    [sym2]: function withName() {},
    [symNoDescription]: function() {},
    [proto]: function() {},

    get [sym3]() {},
    set [b](val) {},
  };

  assertEquals('', obj[''].name);
  assertEquals('a', obj[a].name);
  assertEquals('[1]', obj[sym1].name);
  assertEquals('withName', obj[sym2].name);
  assertEquals('', obj[symNoDescription].name);
  assertEquals('__proto__', obj[proto].name);

  assertEquals('get [3]', Object.getOwnPropertyDescriptor(obj, sym3).get.name);
  assertEquals('set b', Object.getOwnPropertyDescriptor(obj, 'b').set.name);

  var objMethods = {
    ['']() {},
    [a]() {},
    [sym1]() {},
    [symNoDescription]() {},
    [proto]() {},
  };

  assertEquals('', objMethods[''].name);
  assertEquals('a', objMethods[a].name);
  assertEquals('[1]', objMethods[sym1].name);
  assertEquals('', objMethods[symNoDescription].name);
  assertEquals('__proto__', objMethods[proto].name);

  class C {
    ['']() { }
    static ''() {}
    [a]() { }
    [sym1]() { }
    static [sym2]() { }
    [symNoDescription]() { }

    get [sym3]() { }
    static set [b](val) { }
  }

  assertEquals('', C.prototype[''].name);
  assertEquals('', C[''].name);
  assertEquals('a', C.prototype[a].name);
  assertEquals('[1]', C.prototype[sym1].name);
  assertEquals('[2]', C[sym2].name);
  assertEquals('', C.prototype[symNoDescription].name);

  assertEquals('get [3]', Object.getOwnPropertyDescriptor(C.prototype, sym3).get.name);
  assertEquals('set b', Object.getOwnPropertyDescriptor(C, 'b').set.name);
})();


(function testAssignment() {
  var basicFn, arrowFn, generatorFn, classLit;

  basicFn = function() { return true; };
  assertEquals('basicFn', basicFn.name);
  var basicFn2 = basicFn;
  assertEquals('basicFn', basicFn2.name);
  basicFn = function functionWithName() { };
  assertEquals("functionWithName", basicFn.name);

  arrowFn = x => x;
  assertEquals('arrowFn', arrowFn.name);
  var arrowFn2 = arrowFn;
  assertEquals('arrowFn', arrowFn2.name);

  generatorFn = function*() { yield true; };
  assertEquals('generatorFn', generatorFn.name);
  var generatorFn2 = generatorFn;
  assertEquals('generatorFn', generatorFn2.name);
  generatorFn = function* generatorWithName() { };
  assertEquals("generatorWithName", generatorFn.name);

  classLit = class { constructor() {} };
  assertEquals('classLit', classLit.name);
  var classLit2 = classLit;
  assertEquals('classLit', classLit2.name);
  classLit = class classWithName { constructor() {} };
  assertEquals('classWithName', classLit.name);
  classLit = class { constructor() {} static name() {} };
  assertEquals('function', typeof classLit.name);
  classLit = class { constructor() {} static get name() { return true; } };
  assertTrue(classLit.name);
  classLit = class { constructor() {} static ['name']() {} };
  assertEquals('function', typeof classLit.name);
  classLit = class { constructor() {} static get ['name']() { return true; } };
  assertTrue(classLit.name);
})();

(function testObjectBindingPattern() {
  var {
    a = function() {},
    b = () => {},
    x = function withName() { },
    y = class { },
    z = class ClassName { },
    q = class { static name() { return 42 } },
    foo: bar = function() {},
    inParens = (() => {}),
    inManyParens = ((((() => {})))),
  } = {};
  assertEquals('a', a.name);
  assertEquals('b', b.name);
  assertEquals('withName', x.name);
  assertEquals('y', y.name);
  assertEquals('ClassName', z.name);
  assertEquals('function', typeof q.name);
  assertEquals('bar', bar.name);
  assertEquals('inParens', inParens.name)
  assertEquals('inManyParens', inManyParens.name)
})();

(function testArrayBindingPattern() {
  var [
    a = function() {},
    b = () => {},
    x = function withName() { },
    y = class { },
    z = class ClassName { },
    q = class { static name() { return 42 } },
    inParens = (() => {}),
    inManyParens = ((((() => {})))),
  ] = [];
  assertEquals('a', a.name);
  assertEquals('b', b.name);
  assertEquals('withName', x.name);
  assertEquals('y', y.name);
  assertEquals('ClassName', z.name);
  assertEquals('function', typeof q.name);
  assertEquals('inParens', inParens.name)
  assertEquals('inManyParens', inManyParens.name)
})();

(function testObjectAssignmentPattern() {
  var a, b, x, y, z, q;
  ({
    a = function() {},
    b = () => {},
    x = function withName() { },
    y = class { },
    z = class ClassName { },
    q = class { static name() { return 42 } },
    foo: bar = function() {},
    inParens = (() => {}),
    inManyParens = ((((() => {})))),
  } = {});
  assertEquals('a', a.name);
  assertEquals('b', b.name);
  assertEquals('withName', x.name);
  assertEquals('y', y.name);
  assertEquals('ClassName', z.name);
  assertEquals('function', typeof q.name);
  assertEquals('bar', bar.name);
  assertEquals('inParens', inParens.name)
  assertEquals('inManyParens', inManyParens.name)
})();

(function testArrayAssignmentPattern() {
  var a, b, x, y, z, q;
  [
    a = function() {},
    b = () => {},
    x = function withName() { },
    y = class { },
    z = class ClassName { },
    q = class { static name() { return 42 } },
    inParens = (() => {}),
    inManyParens = ((((() => {})))),
  ] = [];
  assertEquals('a', a.name);
  assertEquals('b', b.name);
  assertEquals('withName', x.name);
  assertEquals('y', y.name);
  assertEquals('ClassName', z.name);
  assertEquals('function', typeof q.name);
  assertEquals('inParens', inParens.name)
  assertEquals('inManyParens', inManyParens.name)
})();

(function testParameterDestructuring() {
  (function({ a = function() {},
              b = () => {},
              x = function withName() { },
              y = class { },
              z = class ClassName { },
              q = class { static name() { return 42 } },
              foo: bar = function() {},
              inParens = (() => {}),
              inManyParens = ((((() => {})))) }) {
    assertEquals('a', a.name);
    assertEquals('b', b.name);
    assertEquals('withName', x.name);
    assertEquals('y', y.name);
    assertEquals('ClassName', z.name);
    assertEquals('function', typeof q.name);
    assertEquals('bar', bar.name);
    assertEquals('inParens', inParens.name)
    assertEquals('inManyParens', inManyParens.name)
  })({});

  (function([ a = function() {},
              b = () => {},
              x = function withName() { },
              y = class { },
              z = class ClassName { },
              q = class { static name() { return 42 } },
              inParens = (() => {}),
              inManyParens = ((((() => {})))) ]) {
    assertEquals('a', a.name);
    assertEquals('b', b.name);
    assertEquals('withName', x.name);
    assertEquals('y', y.name);
    assertEquals('ClassName', z.name);
    assertEquals('function', typeof q.name);
    assertEquals('inParens', inParens.name)
    assertEquals('inManyParens', inManyParens.name)
  })([]);
})();

(function testDefaultParameters() {
  (function(a = function() {},
            b = () => {},
            x = function withName() { },
            y = class { },
            z = class ClassName { },
            q = class { static name() { return 42 } },
            inParens = (() => {}),
            inManyParens = ((((() => {}))))) {
    assertEquals('a', a.name);
    assertEquals('b', b.name);
    assertEquals('withName', x.name);
    assertEquals('y', y.name);
    assertEquals('ClassName', z.name);
    assertEquals('function', typeof q.name);
    assertEquals('inParens', inParens.name)
    assertEquals('inManyParens', inManyParens.name)
  })();
})();

(function testComputedNameNotShared() {
  function makeClass(propName) {
    return class {
      static [propName]() {}
    }
  }

  var sym1 = Symbol('1');
  var sym2 = Symbol('2');
  var class1 = makeClass(sym1);
  assertEquals('[1]', class1[sym1].name);
  var class2 = makeClass(sym2);
  assertEquals('[2]', class2[sym2].name);
  assertEquals('[1]', class1[sym1].name);
})();


(function testComputedNamesOnlyAppliedSyntactically() {
  function factory() { return () => {}; }

  var obj = { ['foo']: factory() };
  assertEquals('', obj.foo.name);
})();


(function testNameNotReflectedInToString() {
  var f = function () {};
  var g = function* () {};
  var obj = {
    ['h']: function () {},
    i: () => {}
  };
  assertEquals('function () {}', f.toString());
  assertEquals('function* () {}', g.toString());
  assertEquals('function () {}', obj.h.toString());
  assertEquals('() => {}', obj.i.toString());
})();

(function testClassNameOrder() {
  assertEquals(['length', 'prototype'], Object.getOwnPropertyNames(class {}));

  var tmp = {'': class {}};
  var Tmp = tmp[''];
  assertEquals(['length', 'prototype', 'name'], Object.getOwnPropertyNames(Tmp));

  var name = () => '';
  var tmp = {[name()]: class {}};
  var Tmp = tmp[name()];
  assertEquals(['length', 'prototype', 'name'], Object.getOwnPropertyNames(Tmp));

  class A { }
  assertEquals(['length', 'prototype', 'name'], Object.getOwnPropertyNames(A));

  class B { static foo() { } }
  assertEquals(['length', 'prototype', 'foo', 'name'], Object.getOwnPropertyNames(B));

  class C { static name() { } static foo() { } }
  assertEquals(['length', 'prototype', 'name', 'foo'], Object.getOwnPropertyNames(C));
})();

(function testStaticName() {
  class C { static name() { return 42; } }
  assertEquals(42, C.name());
  assertEquals(undefined, new C().name);

  class D { static get name() { return 17; } }
  assertEquals(17, D.name);
  assertEquals(undefined, new D().name);

  var c = class { static name() { return 42; } }
  assertEquals(42, c.name());
  assertEquals(undefined, new c().name);

  var d = class { static get name() { return 17; } }
  assertEquals(17, d.name);
  assertEquals(undefined, new d().name);
})();

(function testNonStaticName() {
  class C { name() { return 42; } }
  assertEquals('C', C.name);
  assertEquals(42, new C().name());

  class D { get name() { return 17; } }
  assertEquals('D', D.name);
  assertEquals(17, new D().name);

  var c = class { name() { return 42; } }
  assertEquals('c', c.name);
  assertEquals(42, new c().name());

  var d = class { get name() { return 17; } }
  assertEquals('d', d.name);
  assertEquals(17, new d().name);
})();
