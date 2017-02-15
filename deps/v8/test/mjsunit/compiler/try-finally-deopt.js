// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function DeoptimizeFinallyFallThrough() {
  var global = 0;
  function f() {
    var a = 1;
    try {
      global = 1;
    } finally {
      global = 42;
      %DeoptimizeNow();
    }
    return global + a;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(43, f());
  assertEquals(42, global);
})();

(function DeoptimizeFinallyReturn() {
  var global = 0;
  function f() {
    try {
      return 10;
    } finally {
      global = 42;
      %DeoptimizeNow();
    }
    return 1;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(10, f());
  assertEquals(42, global);
})();

(function DeoptimizeFinallyReturnDoublyNested() {
  var global = 0;
  function f() {
    try {
      try {
        return 10;
      } finally {
        global += 21;
        %DeoptimizeNow();
      }
    } finally {
      global += 21;
    }
    return 1;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  global = 0;
  assertEquals(10, f());
  assertEquals(42, global);
})();

(function DeoptimizeOuterFinallyReturnDoublyNested() {
  var global = 0;
  function f() {
    try {
      try {
        return 10;
      } finally {
        global += 21;
      }
    } finally {
      global += 21;
      %DeoptimizeNow();
    }
    return 1;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  global = 0;
  assertEquals(10, f());
  assertEquals(42, global);
})();

(function DeoptimizeFinallyThrow() {
  var global = 0;
  function f() {
    try {
      global = 21;
      throw 1;
      global = 2;
    } finally {
      global += 21;
      %DeoptimizeNow();
    }
    global = 3;
    return 1;
  }

  try { f(); } catch(e) {}
  try { f(); } catch(e) {}
  %OptimizeFunctionOnNextCall(f);
  assertThrowsEquals(f, 1);
  assertEquals(42, global);
})();

(function DeoptimizeFinallyThrowNested() {
  var global = 0;
  function f() {
    try {
      try {
        global = 10;
        throw 1;
        global = 2;
      } finally {
        global += 11;
        %DeoptimizeNow();
      }
      global = 4;
    } finally {
      global += 21;
    }
    global = 3;
    return 1;
  }

  try { f(); } catch(e) {}
  try { f(); } catch(e) {}
  %OptimizeFunctionOnNextCall(f);
  assertThrowsEquals(f, 1);
  assertEquals(42, global);
})();

(function DeoptimizeFinallyContinue() {
  var global = 0;
  function f() {
    global = 0;
    for (var i = 0; i < 2; i++) {
      try {
        if (i == 0) continue;
        global += 10;
      } finally {
        global += 6;
        %DeoptimizeNow();
      }
      global += 20;
    }
    return 1;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1, f());
  assertEquals(42, global);
})();

(function DeoptimizeFinallyContinueNestedTry() {
  var global = 0;
  function f() {
    global = 0;
    for (var i = 0; i < 2; i++) {
      try {
        try {
          if (i == 0) continue;
          global += 5;
        } finally {
          global += 4;
          %DeoptimizeNow();
        }
        global += 5;
      } finally {
        global += 2;
      }
      global += 20;
    }
    return 1;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1, f());
  assertEquals(42, global);
})();

(function DeoptimizeFinallyBreak() {
  var global = 0;
  function f() {
    global = 0;
    for (var i = 0; i < 2; i++) {
      try {
        global += 20;
        if (i == 0) break;
        global += 5;
      } finally {
        global += 22;
        %DeoptimizeNow();
      }
      global += 5;
    }
    return 1;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1, f());
  assertEquals(42, global);
})();

(function DeoptimizeFinallyBreakNested() {
  var global = 0;
  function f() {
    global = 0;
    for (var i = 0; i < 2; i++) {
      try {
        try {
          global += 20;
          if (i == 0) break;
          global += 5;
        } finally {
          global += 12;
          %DeoptimizeNow();
        }
        global += 8;
      } finally {
        global += 10;
      }
      global += 5;
    }
    return 1;
  }

  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1, f());
  assertEquals(42, global);
})();
