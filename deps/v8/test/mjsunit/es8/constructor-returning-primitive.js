// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-restrict-constructor-return

assertThrows(
  () => {
    new class {
      constructor() {
        return 1;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class {
      constructor() {
        return 2147483649;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class {
      constructor() {
        return true;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class {
      constructor() {
        return null;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class {
      constructor() {
        return "wat";
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class {
      constructor() {
        return Symbol();
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class {
      constructor() {
        return 2.2;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {
        return 1;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {
        return 2147483649;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {
        return true;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {
        return null;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {
        return "wat";
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {
        return Symbol();
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {
        return 2.2;
      }
    }();
  },
  TypeError,
  "Class constructors may only return object or undefined"
);

assertThrows(
  () => {
    new class extends Object {
      constructor() {}
    }();
  },
  ReferenceError,
  "Must call super constructor in derived class before accessing " +
    "'this' or returning from derived constructor"
);

(function() {
  let ret_val = { x: 1 };
  let x = new class {
    constructor() {
      return ret_val;
    }
  }();
  assertSame(ret_val, x);
})();

(function() {
  class Foo {
    constructor() {}
  }
  let x = new Foo();
  assertTrue(x instanceof Foo);
})();

(function() {
  class Foo {
    constructor() {
      return undefined;
    }
  }
  let x = new Foo();
  assertTrue(x instanceof Foo);
})();

(function() {
  let ret_val = { x: 1 };
  let x = new class extends Object {
    constructor() {
      return ret_val;
    }
  }();
  assertSame(ret_val, x);
})();

(function() {
  class Foo extends Object {
    constructor() {
      super();
      return undefined;
    }
  }

  let x = new Foo();
  assertTrue(x instanceof Foo);
})();

(function() {
  class Foo extends Object {
    constructor() {
      super();
    }
  }

  let x = new Foo();
  assertTrue(x instanceof Foo);
})();

(function() {
  function foo() {
    return 1;
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  function foo() {
    return 2147483649;
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  function foo() {
    return true;
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  function foo() {
    return undefined;
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  function foo() {
    return null;
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  function foo() {
    return "wat";
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  function foo() {
    return Symbol();
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  function foo() {
    return 2.2;
  }
  let x = new foo();
  assertTrue(x instanceof foo);
})();

(function() {
  var ret_val = { x: 1 };
  function foo() {
    return ret_val;
  }
  let x = new foo();
  assertSame(x, ret_val);
})();
