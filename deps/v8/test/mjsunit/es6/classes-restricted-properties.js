// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestThisAccessRestriction() {
  class Base {}
  (function() {
    class C extends Base {
      constructor() {
        var y;
        super();
      }
    }; new C();
  }());
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(this.x);
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(this);
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super.method();
        super(this);
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(super.method());
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(super());
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(1, 2, Object.getPrototypeOf(this));
      }
    }; new C();
  }, ReferenceError);
  (function() {
    class C extends Base {
      constructor() {
        { super(1, 2); }
      }
    }; new C();
  }());
  (function() {
    class C extends Base {
      constructor() {
        if (1) super();
      }
    }; new C();
  }());

  class C1 extends Object {
    constructor() {
      'use strict';
      super();
    }
  };
  new C1();

  class C2 extends Object {
    constructor() {
      ; 'use strict';;;;;
      super();
    }
  };
  new C2();

  class C3 extends Object {
    constructor() {
      ; 'use strict';;;;;
      // This is a comment.
      super();
    }
  };
  new C3();
}());


function testClassRestrictedProperties(C) {
  assertEquals(false, C.hasOwnProperty("arguments"));
  assertThrows(function() { return C.arguments; }, TypeError);
  assertThrows(function() { C.arguments = {}; }, TypeError);

  assertEquals(false, C.hasOwnProperty("caller"));
  assertThrows(function() { return C.caller; }, TypeError);
  assertThrows(function() { C.caller = {}; }, TypeError);

  assertEquals(false, (new C).method.hasOwnProperty("arguments"));
  assertThrows(function() { return new C().method.arguments; }, TypeError);
  assertThrows(function() { new C().method.arguments = {}; }, TypeError);

  assertEquals(false, (new C).method.hasOwnProperty("caller"));
  assertThrows(function() { return new C().method.caller; }, TypeError);
  assertThrows(function() { new C().method.caller = {}; }, TypeError);
}


(function testRestrictedPropertiesStrict() {
  "use strict";
  class ClassWithDefaultConstructor {
    method() {}
  }
  class Class {
    constructor() {}
    method() {}
  }
  class DerivedClassWithDefaultConstructor extends Class {}
  class DerivedClass extends Class { constructor() { super(); } }

  testClassRestrictedProperties(ClassWithDefaultConstructor);
  testClassRestrictedProperties(Class);
  testClassRestrictedProperties(DerivedClassWithDefaultConstructor);
  testClassRestrictedProperties(DerivedClass);
  testClassRestrictedProperties(class { method() {} });
  testClassRestrictedProperties(class { constructor() {} method() {} });
  testClassRestrictedProperties(class extends Class { });
  testClassRestrictedProperties(
      class extends Class { constructor() { super(); } });
})();


(function testRestrictedPropertiesSloppy() {
  class ClassWithDefaultConstructor {
    method() {}
  }
  class Class {
    constructor() {}
    method() {}
  }
  class DerivedClassWithDefaultConstructor extends Class {}
  class DerivedClass extends Class { constructor() { super(); } }

  testClassRestrictedProperties(ClassWithDefaultConstructor);
  testClassRestrictedProperties(Class);
  testClassRestrictedProperties(DerivedClassWithDefaultConstructor);
  testClassRestrictedProperties(DerivedClass);
  testClassRestrictedProperties(class { method() {} });
  testClassRestrictedProperties(class { constructor() {} method() {} });
  testClassRestrictedProperties(class extends Class { });
  testClassRestrictedProperties(
      class extends Class { constructor() { super(); } });
})();
