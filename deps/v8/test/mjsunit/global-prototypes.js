// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


assertEquals(this.__proto__, Object.prototype);

function TestAddingPropertyToGlobalPrototype() {
  let foo_func_called = 0;
  let bar_func_called = 0;

  function Foo() {}
  Foo.prototype.func = function() { ++foo_func_called; }

  delete this.func;
  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Foo.
    o.func();

    // Add the property to Bar which is a Global-mode prototype between o
    // and Foo. In the next iteration, it's looked up from Bar.
    if (i == 9) {
      Bar.prototype.func = function() { ++bar_func_called; }
    }
  }

  assertEquals(10, foo_func_called);
  assertEquals(1, bar_func_called);
}

TestAddingPropertyToGlobalPrototype();


// Same as TestAddingPropertyToGlobalPrototype, but using o["foo"] access
// instead of o.foo.
function TestAddingPropertyToGlobalPrototype2() {
  let foo_func_called = 0;
  let bar_func_called = 0;
  let name = "func";

  function Foo() {}
  Foo.prototype[name] = function() { ++foo_func_called; }

  delete this[name];
  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Foo.
    o[name]();

    // Add the property to Bar which is a Global-mode prototype between o
    // and Foo. In the next iteration, it's looked up from Bar.
    if (i == 9) {
      Bar.prototype[name] = function() { ++bar_func_called; }
    }
  }

  assertEquals(10, foo_func_called);
  assertEquals(1, bar_func_called);
}

TestAddingPropertyToGlobalPrototype2();


function TestAddingPropertyToGlobalPrototype_DefineProperty() {
  let foo_func_called = 0;
  let bar_func_called = 0;

  function Foo() {}
  Foo.prototype.func = function() { ++foo_func_called; }

  delete this.func;
  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Foo.
    o.func();

    // Add the property to Bar which is a Global-mode prototype between o
    // and Foo. In the next iteration, it's looked up from Bar.
    if (i == 9) {
      Object.defineProperty(Bar.prototype, "func",
                            {
                              value: function() { ++bar_func_called; },
                              configurable:true
                            });
    }
  }

  assertEquals(10, foo_func_called);
  assertEquals(1, bar_func_called);
}

TestAddingPropertyToGlobalPrototype_DefineProperty();


function TestAddingAccessorPropertyToGlobalPrototype() {
  let foo_func_called = 0;
  let bar_func_called = 0;

  function Foo() {}
  Foo.prototype.func = function() { ++foo_func_called; }

  delete this.func;
  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Foo.
    o.func();

    // Add the property to Bar which is a Global-mode prototype between o
    // and Foo. In the next iteration, it's looked up from Bar.
    if (i == 9) {
      Object.defineProperty(Bar.prototype, "func",
                            {
                              get: function() { return function() { ++bar_func_called; }},
                              configurable: true
                            });
    }
  }

  assertEquals(10, foo_func_called);
  assertEquals(1, bar_func_called);
}

TestAddingAccessorPropertyToGlobalPrototype();


function TestRemovingPropertyFromGlobalPrototype() {
  let foo_func_called = 0;
  let bar_func_called = 0;

  function Foo() {}
  Foo.prototype.func = function() { ++foo_func_called; }

  delete this.func;
  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;
  Bar.prototype.func = function() { ++bar_func_called; }

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Bar.
    o.func();

    // Remove the property from Bar which is a Global-mode prototype between
    // o and Foo. In the next iteration, it's looked up from Foo.
    if (i == 9) {
      delete Bar.prototype.func;
    }
  }

  assertEquals(1, foo_func_called);
  assertEquals(10, bar_func_called);
}

TestRemovingPropertyFromGlobalPrototype();


// Same as TestRemovingPropertyFromGlobalPrototype, but using o["foo"] access
// instead of o.foo.
function TestRemovingPropertyFromGlobalPrototype2() {
  let foo_func_called = 0;
  let bar_func_called = 0;
  let name = "func";

  function Foo() {}
  Foo.prototype[name] = function() { ++foo_func_called; }

  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;
  Bar.prototype[name] = function() { ++bar_func_called; }

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Bar.
    o[name]();

    // Remove the property from Bar which is a Global-mode prototype between
    // o and Foo. In the next iteration, it's looked up from Foo.
    if (i == 9) {
      delete Bar.prototype[name];
    }
  }

  assertEquals(1, foo_func_called);
  assertEquals(10, bar_func_called);
}

TestRemovingPropertyFromGlobalPrototype2();


function TestAddingPropertyToGlobalPrototype_MonomorphicDot() {
  function DoMonomorphicStoreToPrototypeDot(p, f, do_delete=true) {
    p.func = f;
    if (do_delete) {
      delete p.func;
    }
  }
  let foo_func_called = 0;
  let bar_func_called = 0;

  function Foo() {}
  Foo.prototype.func = function() { ++foo_func_called; }

  delete this.func;
  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;

  function bar_func() {
    ++bar_func_called;
  }
  DoMonomorphicStoreToPrototypeDot(Bar.prototype, bar_func);
  DoMonomorphicStoreToPrototypeDot(Bar.prototype, bar_func);
  DoMonomorphicStoreToPrototypeDot(Bar.prototype, bar_func);

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Foo.
    o.func();

    // Add the property to Bar which is a Global-mode prototype between o
    // and Foo. In the next iteration, it's looked up from Bar.
    if (i == 9) {
      DoMonomorphicStoreToPrototypeDot(Bar.prototype, bar_func, false);
    }
  }

  assertEquals(10, foo_func_called);
  assertEquals(1, bar_func_called);
}

TestAddingPropertyToGlobalPrototype_MonomorphicDot();


function TestAddingPropertyToGlobalPrototype_MonomorphicBrackets() {
  function DoMonomorphicStoreToPrototypeBrackets(p, name, f, do_delete=true) {
    p[name] = f;
    if (do_delete) {
      delete p[name];
    }
  }
  let foo_func_called = 0;
  let bar_func_called = 0;
  let name = "func";

  function Foo() {}
  Foo.prototype[name] = function() { ++foo_func_called; }

  delete this[name];
  this.__proto__ = Foo.prototype;

  function Bar() {}
  Bar.prototype = this;

  function bar_func() {
    ++bar_func_called;
  }
  DoMonomorphicStoreToPrototypeBrackets(Bar.prototype, name, bar_func);
  DoMonomorphicStoreToPrototypeBrackets(Bar.prototype, name, bar_func);
  DoMonomorphicStoreToPrototypeBrackets(Bar.prototype, name, bar_func);

  let o = new Bar();

  for (let i = 0; i < 11; ++i) {
    // First, the property is looked up from Foo.
    o.func();

    // Add the property to Bar which is a Global-mode prototype between o
    // and Foo. In the next iteration, it's looked up from Bar.
    if (i == 9) {
      DoMonomorphicStoreToPrototypeBrackets(Bar.prototype, name, bar_func, false);
    }
  }

  assertEquals(10, foo_func_called);
  assertEquals(1, bar_func_called);
}

TestAddingPropertyToGlobalPrototype_MonomorphicBrackets();


function TestReconfiguringDataToAccessor() {
  let setter_called = 0;
  let name = "prop";

  delete this[name];
  this.__proto__ = Object.prototype;

  function Bar() {}
  Bar.prototype = this;

  Object.defineProperty(Bar.prototype, name, {value: 1000, writable: true, configurable: true});

  for (let i = 0; i < 11; ++i) {
    let obj1 = new Bar();
    if (i < 10) {
      assertEquals(1000, obj1.prop);
    } else {
      assertEquals(3000, obj1.prop);
    }

    // Add the property into the object.
    obj1.prop = 2000;
    if (i < 10) {
      assertEquals(2000, obj1.prop);
    } else {
      assertEquals(3000, obj1.prop);
    }

    // Make "prop" an accessor property in the prototype.
    if (i == 9) {
      Object.defineProperty(Bar.prototype, name,
                            {get: () => 3000,
                             set: function(val) { ++setter_called; }});
    }
  }
  assertEquals(1, setter_called);
}

TestReconfiguringDataToAccessor();
