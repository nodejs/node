// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestSuperInMethods() {
  class B {
    method() {
      return 1;
    }
    get x() {
      return 2;
    }
  }
  class C extends B {
    method() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, new C().method());
})();


(function TestSuperInGetter() {
  class B {
    method() {
      return 1;
    }
    get x() {
      return 2;
    }
  }
  class C extends B {
    get y() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, new C().y);
})();


(function TestSuperInSetter() {
  class B {
    method() {
      return 1;
    }
    get x() {
      return 2;
    }
  }
  class C extends B {
    set y(v) {
      assertEquals(3, v);
      assertEquals(2, super.x);
      assertEquals(1, super.method());
    }
  }
  assertEquals(3, new C().y = 3);
})();


(function TestSuperInStaticMethods() {
  class B {
    static method() {
      return 1;
    }
    static get x() {
      return 2;
    }
  }
  class C extends B {
    static method() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, C.method());
})();


(function TestSuperInStaticGetter() {
  class B {
    static method() {
      return 1;
    }
    static get x() {
      return 2;
    }
  }
  class C extends B {
    static get x() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, C.x);
})();


(function TestSuperInStaticSetter() {
  class B {
    static method() {
      return 1;
    }
    static get x() {
      return 2;
    }
  }
  class C extends B {
    static set x(v) {
      assertEquals(3, v);
      assertEquals(2, super.x);
      assertEquals(1, super.method());
    }
  }
  assertEquals(3, C.x = 3);
})();
