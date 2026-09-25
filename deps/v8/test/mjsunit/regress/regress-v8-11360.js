// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestCompoundAssignmentToPrivateField() {
  class C {
    #foo = 1;
    m() {
      return this.#foo += 1;
    }
  }

  assertEquals(2, (new C()).m());
})();

(function TestCompoundAssignmentToPrivateFieldWithOnlyGetter() {
  class C {
    get #foo() { return 1; }
    m() {
      return this.#foo += 1;
    }
  }

  assertThrows(() => { (new C()).m(); });
})();

(function TestCompoundAssignmentToPrivateFieldWithOnlySetter() {
  class C {
    set #foo(a) { }
    m() {
      return this.#foo += 1;
    }
  }

  assertThrows(() => { (new C()).m(); });
})();

(function TestCompoundAssignmentToPrivateFieldWithGetterAndSetter() {
  class C {
    get #foo() { return 1; }
    set #foo(a) { }
    m() {
      return this.#foo += 1;
    }
  }

  assertEquals(2, (new C()).m());
})();

(function TestCompoundAssignmentToPrivateMethod() {
  class C {
    m() {
      return this.#pm += 1;
    }
    #pm() {}
  }

  assertThrows(() => { (new O()).m(); });
})();

(function TestCompoundAssignmentToStaticPrivateField() {
  class C {
    static #foo = 1;
    m() {
      return C.#foo += 1;
    }
  }

  assertEquals(2, (new C()).m());
})();

(function TestCompoundAssignmentToStaticPrivateFieldWithOnlyGetter() {
  class C {
    static get #foo() { return 1; }
    m() {
      return C.#foo += 1;
    }
  }

  assertThrows(() => { (new C()).m(); });
})();

(function TestCompoundAssignmentToStaticPrivateFieldWithOnlySetter() {
  class C {
    static set #foo(a) { }
    m() {
      return C.#foo += 1;
    }
  }

  assertThrows(() => { (new C()).m(); });
})();

(function TestCompoundAssignmentToStaticPrivateFieldWithGetterAndSetter() {
  class C {
    static get #foo() { return 1; }
    static set #foo(a) { }
    m() {
      return C.#foo += 1;
    }
  }

  assertEquals(2, (new C()).m());
})();

(function TestCompoundAssignmentToStaticPrivateMethod() {
  class C {
    m() {
      return C.#pm += 1;
    }
    static #pm() {}
  }

  assertThrows(() => { (new O()).m(); });
})();

// The following tests test the above cases w/ brand check failures.

(function TestBrandCheck_CompoundAssignmentToPrivateField() {
  class C {
    #foo = 1;
    m() {
      return this.#foo += 1;
    }
  }

  assertThrows(() => { C.prototype.m.call({}); }, TypeError,
               /Cannot read private member/);

  // It's the same error we get from this case:
  class C2 {
    #foo = 1;
    m() {
      return this.#foo;
    }
  }

  assertThrows(() => { C2.prototype.m.call({}); }, TypeError,
               /Cannot read private member/);
})();

(function TestBrandCheck_CompoundAssignmentToPrivateFieldWithOnlyGetter() {
  class C {
    get #foo() { return 1; }
    m() {
      return this.#foo += 1;
    }
  }

  assertThrows(() => { C.prototype.m.call({}); }, TypeError,
               /Receiver must be an instance of class/);

  // It's the same error we get from this case:
  class C2 {
    get #foo() { return 1; }
    m() {
      return this.#foo;
    }
  }

  assertThrows(() => { C2.prototype.m.call({}); }, TypeError,
               /Receiver must be an instance of class/);
})();

(function TestBrandCheck_CompoundAssignmentToPrivateFieldWithOnlySetter() {
  class C {
    set #foo(a) { }
    m() {
      return this.#foo += 1;
    }
  }

  assertThrows(() => { C.prototype.m.call({}); }, TypeError,
               /Receiver must be an instance of class/);
})();

(function TestBrandCheck_CompoundAssignmentToPrivateFieldWithGetterAndSetter() {
  class C {
    get #foo() { return 1; }
    set #foo(a) { }
    m() {
      return this.#foo += 1;
    }
  }

  assertThrows(() => { C.prototype.m.call({}); }, TypeError,
               /Receiver must be an instance of class/);

  // It's the same error we get from this case:
  class C2 {
    get #foo() { return 1; }
    set #foo(a) { }
    m() {
      return this.#foo;
    }
  }

  assertThrows(() => { C2.prototype.m.call({}); }, TypeError,
               /Receiver must be an instance of class/);
})();

(function TestBrandCheck_CompoundAssignmentToPrivateMethod() {
  class C {
    m() {
      return this.#pm += 1;
    }
    #pm() {}
  }

  assertThrows(() => { C.prototype.m.call({}); }, TypeError,
               /Receiver must be an instance of class/);
})();
