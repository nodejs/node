// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --harmony-arrow-functions
"use strict"

let prologue_dead = "(function outer() { if (false) { ";
let epilogue_dead = " } })();";

let prologue_live = "(function outer() { ";
let epilogue_live = "})();";

// For code which already throws a run-time error in non-strong mode; we assert
// that we now get the error already compilation time.
function assertLateErrorsBecomeEarly(code) {
  assertThrows("'use strong'; " + prologue_dead + code + epilogue_dead,
               ReferenceError);

  // Make sure the error happens only in strong mode (note that we need strict
  // mode here because of let).
  assertDoesNotThrow("'use strict'; " + prologue_dead + code + epilogue_dead);

  // But if we don't put the references inside a dead code, it throws a run-time
  // error (also in strict mode).
  assertThrows("'use strong'; " + prologue_live + code + epilogue_live,
               ReferenceError);
  assertThrows("'use strict'; " + prologue_live + code + epilogue_live,
               ReferenceError);
}

// For code which doesn't throw an error at all in non-strong mode.
function assertNonErrorsBecomeEarly(code) {
  assertThrows("'use strong'; " + prologue_dead + code + epilogue_dead,
               ReferenceError);
  assertDoesNotThrow("'use strict'; " + prologue_dead + code + epilogue_dead);

  assertThrows("'use strong'; " + prologue_live + code + epilogue_live,
               ReferenceError);
  assertDoesNotThrow("'use strict'; " + prologue_live + code + epilogue_live,
                     ReferenceError);
}

(function InitTimeReferenceForward() {
  // It's never OK to have an init time reference to a class which hasn't been
  // declared.
  assertLateErrorsBecomeEarly(
      `class A extends B { }
      class B {}`);

  assertLateErrorsBecomeEarly(
      `class A {
        [B.sm()]() { }
      }
      class B {
        static sm() { return 0; }
      }`);
})();

(function InitTimeReferenceBackward() {
  // Backwards is of course fine.
  "use strong";
  class A {
    static sm() { return 0; }
  }
  let i = "making these classes non-consecutive";
  class B extends A {};
  "by inserting statements and declarations in between";
  class C {
    [A.sm()]() { }
  };
})();

(function BasicMutualRecursion() {
  "use strong";
  class A {
    m() { B; }
    static sm() { B; }
  }
  // No statements or declarations between the classes.
  class B {
    m() { A; }
    static sm() { A; }
  }
})();

(function MutualRecursionWithMoreClasses() {
  "use strong";
  class A {
    m() { B; C; }
    static sm() { B; C; }
  }
  class B {
    m() { A; C; }
    static sm() { A; C; }
  }
  class C {
    m() { A; B; }
    static sm() { A; B; }
  }
})();

(function ReferringForwardInDeeperScopes() {
  "use strong";

  function foo() {
    class A1 {
      m() { B1; }
    }
    class B1 { }
  }

  class Outer {
    m() {
      class A2 {
        m() { B2; }
      }
      class B2 { }
    }
  }

  for (let i = 0; i < 1; ++i) {
    class A3 {
      m() { B3; }
    }
    class B3 { }
  }

  (a, b) => {
    class A4 {
      m() { B4; }
    }
    class B4 { }
  }
})();

(function ReferringForwardButClassesNotConsecutive() {
  assertNonErrorsBecomeEarly(
      `class A {
        m() { B; }
      }
      ;
      class B {}`);

  assertNonErrorsBecomeEarly(
      `let A = class {
        m() { B; }
      }
      class B {}`);

  assertNonErrorsBecomeEarly(
      `class A {
        m() { B1; } // Just a normal use-before-declaration.
      }
      let B1 = class B2 {}`);

  assertNonErrorsBecomeEarly(
      `class A {
        m() { B; }
      }
      let i = 0;
      class B {}`);

  assertNonErrorsBecomeEarly(
      `class A {
        m() { B; }
      }
      function foo() {}
      class B {}`);

  assertNonErrorsBecomeEarly(
      `function foo() {
        class A {
          m() { B; }
        }
      }
      class B {}`);

  assertNonErrorsBecomeEarly(
      `class A extends class B { m() { C; } } {
      }
      class C { }`);

  assertLateErrorsBecomeEarly(
      `class A extends class B { [C.sm()]() { } } {
      }
      class C { static sm() { return 'a';} }`);

  assertLateErrorsBecomeEarly(
      `class A extends class B extends C { } {
      }
      class C { }`);
})();


(function RegressionForClassResolution() {
  assertNonErrorsBecomeEarly(
      `let A = class B {
        m() { C; }
      }
      ;;;;
      class C {}
      class B {}`);
})();


(function TestMultipleMethodScopes() {
  "use strong";

  // Test cases where the reference is inside multiple method scopes.
  class A1 {
    m() {
      class C1 {
        m() { B1; }
      }
    }
  }
  class B1 { }

  ;

  class A2 {
    m() {
      class C2 extends B2 {
      }
    }
  }
  class B2 { }
})();
