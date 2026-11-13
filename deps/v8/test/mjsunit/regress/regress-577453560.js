// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --compile-hints-magic --stress-background-compile --stress-flush-code
// Flags: --expose-gc

function flush() {
  for (let i = 0; i < 10; i++) gc({type: "major"});
}

// 1. Contextless arrow calling eval inside a function with context.
{
  let code = `//# allFunctionsCalledOnLoad
  "use strict";
  function outer() {
    return function(x) {
      return () => eval("x");
    };
  }
  outer;`;

  let o = Realm.eval(Realm.current(), code);
  flush();
  let f = o()(42);
  Realm.eval(Realm.current(), code);
  assertEquals(42, f());
}

// 2. Contextless arrow calling eval inside a lexical block scope, catch scope,
// and default parameter scope.
{
  let code = `//# allFunctionsCalledOnLoad
  "use strict";
  function outer() {
    return [
      function() {
        {
          let x = 10;
          return () => eval("x");
        }
      },
      function() {
        try {
          throw 20;
        } catch (x) {
          return () => eval("x");
        }
      },
      function(x = 30, f = () => eval("x")) {
        return f;
      },
      function(x) {
        return () => () => eval("() => eval('x')");
      }
    ];
  }
  outer;`;

  let o = Realm.eval(Realm.current(), code);
  flush();
  let [fBlock, fCatch, fParam, fMulti] = o();
  let b = fBlock();
  let c = fCatch();
  let p = fParam();
  let m = fMulti(40)();
  Realm.eval(Realm.current(), code);
  assertEquals(10, b());
  assertEquals(20, c());
  assertEquals(30, p());
  assertEquals(40, m()());
}

// 3. Standalone block-scoped eval merged while the owning closure's ScopeInfo
// is kept alive by a sibling closure.
{
  let code = `//# allFunctionsCalledOnLoad
  "use strict";
  function outer(x) {
    let keepScopeAlive = () => x;
    let runBlockEval = function() {
      {
        let y = 12;
        return eval("x + y");
      }
    };
    return [keepScopeAlive, runBlockEval];
  }
  outer;`;

  let o = Realm.eval(Realm.current(), code);
  let [keep, run] = o(30);
  flush();
  let o2 = Realm.eval(Realm.current(), code);
  assertEquals(30, keep());
  assertEquals(42, run());
  assertEquals(42, o2(30)[1]());
}
