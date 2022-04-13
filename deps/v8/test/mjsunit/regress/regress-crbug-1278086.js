// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

{
  class C {
    field = c.concat();
  }

  var c;
  assertThrows(() => {
    c = new C();
  }, TypeError);
}

// Anonymous class
{
  const C = class {
    field = c.concat();
  }

  var c;
  assertThrows(() => {
    c = new C();
  }, TypeError);
}

class D {
  field = ({ d } = undefined);
}

var d;
assertThrows(
  () => {
    d = new D();
  },
  TypeError,
  /Cannot destructure property 'd' of 'undefined' as it is undefined/);

class B {
  static B = class B {
    field = b.concat();
  }
  static func() {
    return B;  // keep the context for class B
  }
}
var b;
assertThrows(() => {
  b = new B.B();
}, TypeError);

class A {
  static B = class B {
    field = a.concat();
  }
  static func() {
    return A;  // keep the context for class A
  }
}
var a;
assertThrows(() => {
  a = new A.B();
}, TypeError);

class E {
  #x = 1;
  static B = class B {
    field = this.#x;
  }
}

var e;
assertThrows(
  () => { e = new E.B(); },
  TypeError,
  /Cannot read private member #x from an object whose class did not declare it/);
