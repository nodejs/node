// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

"use strict";

{
  class C {
    #a() {}
  }
  new C;
}

{
  class C {
    #a() {
      class B {
        #a() {  }
      }
      new B;
    }
  }
  new C;
}

{
  class A {
    #a() {
      class C extends A {
        #c() { }
      }
      new C;
    }
  }

  new A;
}

{
  const C = class {
    #a() { }
  }
  new C;
}

{
  const C = class {
    #a() {
      const B = class {
        #a() {  }
      }
      new B;
    }
  }
  new C;
}

{
  class A {
    constructor(arg) {
      return arg;
    }
  }

  class C extends A {
    #x() { }

    constructor(arg) {
      super(arg);
    }
  }

  // Add the brand twice on the same object.
  let c1 = new C({});
  assertThrows(() => new C(c1), TypeError);
}

{
  // TODO(v8:9177): test extending a class expression that does not have
  // a private method.
  class D extends class {
    #c() {}
  } {
    #d() {}
  }

  class E extends D {
    #e() {}
  }

  new D;
  new E;
}
