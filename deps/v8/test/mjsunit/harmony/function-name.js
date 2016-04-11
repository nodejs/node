// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-function-name

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
    42() { }
    static 43() { }
    get 44() { }
    set 44(val) { }
  };

  assertEquals('a', C.prototype.a.name);
  assertEquals('b', C.b.name);
  var descriptor = Object.getOwnPropertyDescriptor(C.prototype, 'c');
  assertEquals('get c', descriptor.get.name);
  assertEquals('set c', descriptor.set.name);
  assertEquals('42', C.prototype[42].name);
  assertEquals('43', C[43].name);
  var descriptor = Object.getOwnPropertyDescriptor(C.prototype, '44');
  assertEquals('get 44', descriptor.get.name);
  assertEquals('set 44', descriptor.set.name);
})();

// TODO(adamk): Make computed property names work.
(function testComputedProperties() {
  'use strict';
  var a = 'a';
  var sym1 = Symbol('1');
  var sym2 = Symbol('2');
  var obj = {
    [a]: function() {},
    [sym1]: function() {},
    [sym2]: function withName() {},
  };

  // Should be 'a'
  assertEquals('', obj[a].name);
  // Should be '[1]'
  assertEquals('', obj[sym1].name);
  assertEquals('withName', obj[sym2].name);

  class C {
    [a]() { }
    [sym1]() { }
    static [sym2]() { }
  }

  // Should be 'a'
  assertEquals('', C.prototype[a].name);
  // Should be '[1]'
  assertEquals('', C.prototype[sym1].name);
  // Should be '[2]'
  assertEquals('', C[sym2].name);
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
