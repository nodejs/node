// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-specialize-for-script-context --maglev
// Flags: --allow-natives-syntax

const scriptConst = 42;

function createClosure() {
  return function foo() {
    return scriptConst;
  };
}

// Create the closure twice to avoid function context specialization.
const f1 = createClosure();
const f2 = createClosure();

%PrepareFunctionForOptimization(f1);
assertEquals(42, f1());
%OptimizeMaglevOnNextCall(f1);
assertEquals(42, f1());
assertMaglevved(f1);

%PrepareFunctionForOptimization(f2);
assertEquals(42, f2());
%OptimizeMaglevOnNextCall(f2);
assertEquals(42, f2());
assertMaglevved(f2);

function createClosure2() {
  return function bar() {
    return scriptConst;
  };
}

// Create and optimize the first closure (with function context specialization),
// then create and optimize a second closure (transitioning from
// one_closure_cell to many_closures_cell and recompiling without function
// context specialization).
const g1 = createClosure2();
%PrepareFunctionForOptimization(g1);
assertEquals(42, g1());
%OptimizeMaglevOnNextCall(g1);
assertEquals(42, g1());
assertMaglevved(g1);

const g2 = createClosure2();
%PrepareFunctionForOptimization(g2);
assertEquals(42, g2());
%OptimizeMaglevOnNextCall(g2);
assertEquals(42, g2());
assertMaglevved(g2);
assertEquals(42, g1());
assertMaglevved(g1);

// Test inlining a known constant JSFunction from another script (with its own
// ScriptContext and FunctionContext).
const otherScriptFn = Realm.eval(Realm.current(), `
  const otherScriptConst = 100;
  let otherScriptLet = 200;
  (function makeOther() {
    let otherFuncVar = 300;
    return function otherInlined(x) {
      return otherScriptConst + otherScriptLet + otherFuncVar + x;
    };
  })();
`);

function callOtherScriptFn(x) {
  return otherScriptFn(x) + scriptConst;
}
%PrepareFunctionForOptimization(otherScriptFn);
%PrepareFunctionForOptimization(callOtherScriptFn);
assertEquals(643, callOtherScriptFn(1));
assertEquals(644, callOtherScriptFn(2));
%OptimizeMaglevOnNextCall(callOtherScriptFn);
assertEquals(645, callOtherScriptFn(3));
assertMaglevved(callOtherScriptFn);

// Test inlining a closure created via FastCreateClosure inside a non-FCI caller
// and inside a many-closures caller.
function makeCallerWithFastCreateClosure(base) {
  return function caller(x) {
    const inner = (y) => base + scriptConst + y;
    return inner(x);
  };
}
const c1 = makeCallerWithFastCreateClosure(10);
%PrepareFunctionForOptimization(c1);
assertEquals(53, c1(1));
assertEquals(54, c1(2));
%OptimizeMaglevOnNextCall(c1);
assertEquals(55, c1(3));
assertMaglevved(c1);

const c2 = makeCallerWithFastCreateClosure(20);
%PrepareFunctionForOptimization(c2);
assertEquals(63, c2(1));
assertEquals(64, c2(2));
%OptimizeMaglevOnNextCall(c2);
assertEquals(65, c2(3));
assertMaglevved(c2);
assertEquals(55, c1(3));

// Test inlining via FeedbackCell (dynamic closure with different
// FunctionContext instances, sharing the same ScriptContext).
function makeDynamicInner(secret) {
  return function dynamicInner(x) {
    return secret + scriptConst + x;
  };
}
const dyn1 = makeDynamicInner(100);
const dyn2 = makeDynamicInner(200);
function callDynamicInner(fn, x) {
  return fn(x);
}
%PrepareFunctionForOptimization(dyn1);
%PrepareFunctionForOptimization(dyn2);
%PrepareFunctionForOptimization(callDynamicInner);
assertEquals(143, callDynamicInner(dyn1, 1));
assertEquals(244, callDynamicInner(dyn2, 2));
%OptimizeMaglevOnNextCall(callDynamicInner);
assertEquals(145, callDynamicInner(dyn1, 3));
assertEquals(246, callDynamicInner(dyn2, 4));
assertMaglevved(callDynamicInner);

// Test OSR compilation with script context specialization.
function createClosureOsr() {
  return function osrSimple(n) {
    let sum = 0;
    for (let i = 0; i < n; i++) {
      if (i == 5) %OptimizeOsr();
      sum += scriptConst;
    }
    return sum;
  };
}

const osr1 = createClosureOsr();
const osr2 = createClosureOsr();
%PrepareFunctionForOptimization(osr1);
assertEquals(420, osr1(10));
%PrepareFunctionForOptimization(osr2);
assertEquals(420, osr2(10));

// Test OSR when an inner FunctionContext is pushed before the OSR loop.
function createClosureOsrInnerContext() {
  return function osrInner(n) {
    let captured = 10;
    let getCaptured = () => captured;
    let sum = 0;
    for (let i = 0; i < n; i++) {
      if (i == 5) %OptimizeOsr();
      sum += scriptConst + getCaptured();
    }
    return sum;
  };
}

const osrInner1 = createClosureOsrInnerContext();
const osrInner2 = createClosureOsrInnerContext();
%PrepareFunctionForOptimization(osrInner1);
assertEquals(520, osrInner1(10));
%PrepareFunctionForOptimization(osrInner2);
assertEquals(520, osrInner2(10));

// Test OSR when a BlockContext is pushed before the OSR loop.
function createClosureOsrBlockContext() {
  return function osrBlock(n) {
    let sum = 0;
    {
      let blockVar = 20;
      let getBlockVar = () => blockVar;
      for (let i = 0; i < n; i++) {
        if (i == 5) %OptimizeOsr();
        sum += scriptConst + getBlockVar();
      }
    }
    return sum;
  };
}

const osrBlock1 = createClosureOsrBlockContext();
const osrBlock2 = createClosureOsrBlockContext();
%PrepareFunctionForOptimization(osrBlock1);
assertEquals(620, osrBlock1(10));
%PrepareFunctionForOptimization(osrBlock2);
assertEquals(620, osrBlock2(10));

// Test OSR inside a catch block (CatchContext pushed before OSR loop).
function createClosureOsrCatchContext() {
  return function osrCatch(n) {
    let sum = 0;
    try {
      throw 1;
    } catch (e) {
      let getE = () => e;
      for (let i = 0; i < n; i++) {
        if (i == 5) %OptimizeOsr();
        sum += scriptConst + getE();
      }
    }
    return sum;
  };
}

const osrCatch1 = createClosureOsrCatchContext();
const osrCatch2 = createClosureOsrCatchContext();
%PrepareFunctionForOptimization(osrCatch1);
assertEquals(430, osrCatch1(10));
%PrepareFunctionForOptimization(osrCatch2);
assertEquals(430, osrCatch2(10));

// Test popping out of nested OSR contexts (PopContext after loop exit) and
// accessing outer contexts (both intermediate contexts and ScriptContext).
function createClosureOsrPopOutNested() {
  return function osrPopOut(n) {
    let funcVar = 100;
    let getFuncVar = () => funcVar;
    let sum = 0;
    {
      let block1Var = 10;
      let getBlock1Var = () => block1Var;
      {
        let block2Var = 1;
        let getBlock2Var = () => block2Var;
        for (let i = 0; i < n; i++) {
          if (i == 5) %OptimizeOsr();
          sum += scriptConst + getBlock2Var();
        }
      }
      // Popped out of block2 (the OSR entry context) into block1!
      sum += scriptConst + getBlock1Var() + getFuncVar();
    }
    // Popped out of block1 into FunctionContext!
    sum += scriptConst + getFuncVar();
    return sum;
  };
}

const osrPop1 = createClosureOsrPopOutNested();
const osrPop2 = createClosureOsrPopOutNested();
%PrepareFunctionForOptimization(osrPop1);
assertEquals(430 + 42 + 10 + 100 + 42 + 100, osrPop1(10));
%PrepareFunctionForOptimization(osrPop2);
assertEquals(430 + 42 + 10 + 100 + 42 + 100, osrPop2(10));

// Test popping out of the OSR context via exception handling (try/catch
// outside the OSR block context).
function createClosureOsrPopOutException() {
  return function osrPopException(n) {
    let funcVar = 200;
    let getFuncVar = () => funcVar;
    let sum = 0;
    try {
      {
        let blockVar = 30;
        let getBlockVar = () => blockVar;
        for (let i = 0; i < n; i++) {
          if (i == 5) %OptimizeOsr();
          sum += scriptConst + getBlockVar();
          if (i == 8) throw new Error("boom");
        }
      }
    } catch (e) {
      // Caught outside the inner BlockContext; current context is CatchContext
      // whose outer is FunctionContext -> ScriptContext.
      sum += scriptConst + getFuncVar();
    }
    // Popped out of CatchContext back to FunctionContext.
    sum += scriptConst + getFuncVar();
    return sum;
  };
}

const osrExc1 = createClosureOsrPopOutException();
const osrExc2 = createClosureOsrPopOutException();
%PrepareFunctionForOptimization(osrExc1);
assertEquals(9 * (42 + 30) + (42 + 200) + (42 + 200), osrExc1(10));
%PrepareFunctionForOptimization(osrExc2);
assertEquals(9 * (42 + 30) + (42 + 200) + (42 + 200), osrExc2(10));

// Test OSR in a top-level function where specialization_context_distance is 0
// (the function's context is the ScriptContext itself, with no pushed context
// at loop entry).
function toplevelOsrDistanceZero(n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    if (i == 5) %OptimizeOsr();
    sum += scriptConst;
  }
  return sum;
}

%PrepareFunctionForOptimization(toplevelOsrDistanceZero);
assertEquals(420, toplevelOsrDistanceZero(10));

// Regression test: register reuse after PopContext at distance 0 before an OSR
// for..in loop (where ToObject reuses the popped context register without a
// preceding Star).
function toplevelOsrRegisterReuse(obj) {
  {
    let x = 1;
    (() => x)();
  }
  let res = [];
  let i = 0;
  for (let k in obj) {
    if (i++ == 2) %OptimizeOsr();
    res.push(k + scriptConst);
  }
  return res;
}

%PrepareFunctionForOptimization(toplevelOsrRegisterReuse);
assertEquals(
    ['a42', 'b42', 'c42', 'd42', 'e42'],
    toplevelOsrRegisterReuse({a: 1, b: 2, c: 3, d: 4, e: 5}));

// Regression test: branching control flow before an OSR loop where only one
// branch pushes/pops a block context.
function toplevelOsrBranchBeforeLoop(cond, n) {
  if (cond) {
    let x = 10;
    (() => x)();
  }
  let sum = 0;
  for (let i = 0; i < n; i++) {
    if (i == 5) %OptimizeOsr();
    sum += scriptConst;
  }
  return sum;
}

%PrepareFunctionForOptimization(toplevelOsrBranchBeforeLoop);
assertEquals(420, toplevelOsrBranchBeforeLoop(true, 10));
assertEquals(420, toplevelOsrBranchBeforeLoop(false, 10));

// Regression test: inlined function inside an OSR script-context-specialized
// function accessing script context slots.
function inlineHelper(x) {
  return x + scriptConst;
}
%PrepareFunctionForOptimization(inlineHelper);

function toplevelOsrWithInlining(n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    if (i == 5) %OptimizeOsr();
    sum += inlineHelper(i);
  }
  return sum;
}

%PrepareFunctionForOptimization(toplevelOsrWithInlining);
assertEquals(420 + 45, toplevelOsrWithInlining(10));

// Regression test: inlined inner closure inside a factory-created outer
// function (where specialization_context_distance > 0) accessing scriptConst.
function createOuterWithInlinedClosure(capturedParam) {
  return function outerWithInlinedClosure(n) {
    let innerClosure = (x) => x + capturedParam + scriptConst;
    %PrepareFunctionForOptimization(innerClosure);
    let sum = 0;
    for (let i = 0; i < n; i++) {
      sum += innerClosure(i);
    }
    return sum;
  };
}

const inlinedClosure1 = createOuterWithInlinedClosure(10);
const inlinedClosure2 = createOuterWithInlinedClosure(20);
%PrepareFunctionForOptimization(inlinedClosure1);
%PrepareFunctionForOptimization(inlinedClosure2);
assertEquals(45 + 100 + 420, inlinedClosure1(10));
%OptimizeMaglevOnNextCall(inlinedClosure1);
assertEquals(45 + 100 + 420, inlinedClosure1(10));
assertEquals(45 + 200 + 420, inlinedClosure2(10));

// Regression test: inlined closure that pushes a BlockContext across a branch
// (producing a Phi context inside the inlined function) and accesses outer
// function and script context slots.
function createOuterWithBranchingInlinedClosure(capturedParam) {
  return function outerWithBranchingInlinedClosure(n) {
    let innerClosure = (x) => {
      let res = 0;
      {
        let blockVar = x;
        let getBlockVar = () => blockVar;
        if (x & 1) {
          let innerBlockVar = x + 1;
          let getInner = () => innerBlockVar;
          res += getInner();
        }
        // After the `if`, if we had a context phi or inside the block context:
        res += getBlockVar() + capturedParam + scriptConst;
      }
      return res;
    };
    %PrepareFunctionForOptimization(innerClosure);
    let sum = 0;
    for (let i = 0; i < n; i++) {
      sum += innerClosure(i);
    }
    return sum;
  };
}

const branchingInlined1 = createOuterWithBranchingInlinedClosure(10);
%PrepareFunctionForOptimization(branchingInlined1);
assertEquals(30 + 45 + 100 + 420, branchingInlined1(10));
%OptimizeMaglevOnNextCall(branchingInlined1);
assertEquals(30 + 45 + 100 + 420, branchingInlined1(10));

// Regression test: top-level eval (with and without EvalContext) undergoing OSR
// and accessing scriptConst.
const evalResultWithContext = eval(`
  let evalVar = 5;
  let getEvalVar = () => evalVar;
  let evalSum = 0;
  for (let i = 0; i < 10; i++) {
    if (i == 5) %OptimizeOsr();
    evalSum += scriptConst + getEvalVar();
  }
  evalSum;
`);
assertEquals(470, evalResultWithContext);

const evalResultWithoutContext = eval(`
  var evalNoCtxSum = 0;
  for (var j = 0; j < 10; j++) {
    if (j == 5) %OptimizeOsr();
    evalNoCtxSum += scriptConst;
  }
  evalNoCtxSum;
`);
assertEquals(420, evalResultWithoutContext);

// Regression test (crbug.com/569212097): inlining a closure created inside an
// inlined factory call whose FunctionContext has the same ScopeInfo as the
// outer caller's specialization_context.
function outerFactoryWithOuterAccess(createTop, val) {
  let captured = val;
  function inner() {
    return captured + scriptConst;
  }
  if (createTop) {
    return function top() {
      const freshInner = outerFactoryWithOuterAccess(false, 100);
      return freshInner() + captured;
    };
  }
  return inner;
}

const topWithOuterAccess = outerFactoryWithOuterAccess(true, 1);
%PrepareFunctionForOptimization(outerFactoryWithOuterAccess);
%PrepareFunctionForOptimization(topWithOuterAccess);
assertEquals(143, topWithOuterAccess());
assertEquals(143, topWithOuterAccess());
%OptimizeMaglevOnNextCall(topWithOuterAccess);
assertEquals(143, topWithOuterAccess());
assertMaglevved(topWithOuterAccess);

// Same scenario at depth 0 only (immutable const slot in FunctionContext),
// ensuring the fresh closure's FunctionContext is not confused with the
// caller's specialization_context.
function outerFactoryDepthZeroOnly(createTop, val) {
  const immutableCaptured = val;
  function inner() {
    return immutableCaptured;
  }
  if (createTop) {
    return function top() {
      const freshInner = outerFactoryDepthZeroOnly(false, 100);
      return freshInner() + immutableCaptured;
    };
  }
  return inner;
}

const topDepthZeroOnly = outerFactoryDepthZeroOnly(true, 1);
%PrepareFunctionForOptimization(outerFactoryDepthZeroOnly);
%PrepareFunctionForOptimization(topDepthZeroOnly);
assertEquals(101, topDepthZeroOnly());
assertEquals(101, topDepthZeroOnly());
%OptimizeMaglevOnNextCall(topDepthZeroOnly);
assertEquals(101, topDepthZeroOnly());
assertMaglevved(topDepthZeroOnly);

// Three-level nesting: grandOuter -> outerFactory -> inner, where top is
// specialized to the first outerFactory context (C_1) while freshInner is
// created in a second outerFactory context (C_2) that still shares grandOuter's
// context (C_0) and the script context.
function makeGrandOuter(grandVal) {
  const grandCaptured = grandVal;
  function outerFactory(createTop, val) {
    const outerCaptured = val;
    function inner() {
      return outerCaptured + grandCaptured + scriptConst;
    }
    if (createTop) {
      return function top() {
        const freshInner = outerFactory(false, 200);
        return freshInner() + outerCaptured;
      };
    }
    return inner;
  }
  return outerFactory(true, 1);
}

const topThreeLevel = makeGrandOuter(1000);
%PrepareFunctionForOptimization(topThreeLevel);
assertEquals(1243, topThreeLevel());
assertEquals(1243, topThreeLevel());
%OptimizeMaglevOnNextCall(topThreeLevel);
assertEquals(1243, topThreeLevel());
assertMaglevved(topThreeLevel);
