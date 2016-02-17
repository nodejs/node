// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

"use strong";

function testSuper(object) {
  assertEquals(0, object.validLoad());
  assertThrows(function(){ return object.propertyLoad() }, TypeError);
  assertThrows(function(){ return object.elementLoad()  }, TypeError);
  assertThrows(function(){ return object.accessorLoad() }, TypeError);
}

class A {
  constructor() {}
  foo() {
    return 0;
  }
  get bar() {
    return 0;
  }
  set baz(_) {
    return;
  }
}

class B extends A {
  constructor() {
    super();
  }
  validLoad() {
    return super.foo() + super.bar;
  }
  propertyLoad() {
    return super.x;
  }
  elementLoad() {
    return super[1];
  }
  accessorLoad() {
    return super.baz;
  }
}

class C extends A {
  constructor() {
    super();
    this[1] = 0;
    this.x = 0;
  }
  get baz() {
    return 0;
  }
  validLoad() {
    return super.foo() + super.bar;
  }
  propertyLoad() {
    return super.x;
  }
  elementLoad() {
    return super[1];
  }
  accessorLoad() {
    return super.baz;
  }
}

let b = new B();
let c = new C();
testSuper(b);
testSuper(c);

let d = {
  "0": 0,
  foo: 0,
  bar: (function(){return 0}),
  get baz(){return 0},
  set qux(_){return}
}

let e = {
  __proto__: d,
  "1": 0,
  x: 0,
  get baz(){return 0},
  validLoad() {
    return super[0] + super.foo + super.bar() + super.baz;
  },
  propertyLoad() {
    return super.x;
  },
  elementLoad() {
    return super[1];
  },
  accessorLoad() {
    return super.qux;
  }
}

testSuper(e);
