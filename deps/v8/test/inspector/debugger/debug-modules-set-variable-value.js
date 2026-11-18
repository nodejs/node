// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-turbofan

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Debugger.setVariableValue across different scope types in modules.');

const source = `
function ClosureTestCase(scope_index, old_result, variable_name, new_value,
    new_result, success_expected, factory) {
  this.scope_index_ = scope_index;
  this.old_result_ = old_result;
  this.variable_name_ = variable_name;
  this.new_value_ = new_value;
  this.new_result_ = new_result;
  this.success_expected_ = success_expected;
  this.factory_ = factory;
}

ClosureTestCase.prototype.run_pause_test = function() {
  var fun = this.factory_(true);
  this.run_and_catch_(function() {
    return ClosureTestCase;
  });
};

ClosureTestCase.prototype.run_and_catch_ = function(runnable) {
  if (this.success_expected_) {
    runnable();
  }
};

globalThis.closure_test_cases = [
  new ClosureTestCase(0, 'cat', 'v1', 5, 5, true,
      function Factory(debug_stop) {
    var v1 = 'cat';
    return function() {
      if (debug_stop) debugger;
      return v1;
    };
  }),

  new ClosureTestCase(0, 4, 't', 7, 9, true, function Factory(debug_stop) {
    var t = 2;
    var r = eval("t");
    return function() {
      if (debug_stop) debugger;
      return r + t;
    };
  }),

  new ClosureTestCase(0, 6, 't', 10, 13, true, function Factory(debug_stop) {
    var t = 2;
    var r = eval("t = 3");
    return function() {
      if (debug_stop) debugger;
      return r + t;
    };
  }),

  new ClosureTestCase(2, 'capybara', 'foo', 77, 77, true,
      function Factory(debug_stop) {
    var foo = "capybara";
    return (function() {
      var bar = "fish";
      try {
        throw {name: "test exception"};
      } catch (e) {
        return function() {
          if (debug_stop) debugger;
          bar = "beast";
          return foo;
        };
      }
    })();
  }),

  new ClosureTestCase(0, 'AlphaBeta', 'eee', 5, '5Beta', true,
      function Factory(debug_stop) {
    var foo = "Beta";
    return (function() {
      var bar = "fish";
      try {
        throw "Alpha";
      } catch (eee) {
        return function() {
          if (debug_stop) debugger;
          return eee + foo;
        };
      }
    })();
  })
];

globalThis.local_test_cases = [
  {
    scope_number: 0,
    expected_old_result: 'HelloYou',
    variable_name: 'u',
    new_value: 'We',
    expected_new_result: 'HelloWe',
    fun: (function Factory() {
      return function() {
        var u = "You";
        var v = "Hello";
        debugger;
        return v + u;
      };
    })()
  },
  {
    scope_number: 0,
    expected_old_result: 'Helloworld',
    variable_name: 'p',
    new_value: 'GoodBye',
    expected_new_result: 'HelloGoodBye',
    fun: (function Factory() {
      function H(p) {
        var v = "Hello";
        debugger;
        return v + p;
      }
      return function() {
        return H("world");
      };
    })()
  },
  {
    scope_number: 0,
    expected_old_result: 'mouse',
    variable_name: 'v1',
    new_value: 'dog',
    expected_new_result: 'dog',
    fun: (function Factory() {
      return function() {
        var v1 = 'cat';
        eval("v1 = 'mouse'");
        debugger;
        return v1;
      };
    })()
  },
  {
    scope_number: 0,
    expected_old_result: 'Blue',
    variable_name: 'v',
    new_value: 'Green',
    expected_new_result: 'Green',
    fun: (function Factory() {
      return function() {
        function A() {
          var v = "Blue";
          function Inner() {
            return void v;
          }
          debugger;
          return v;
        }
        return A();
      };
    })()
  },
  {
    scope_number: 0,
    expected_old_result: 5,
    variable_name: 'p',
    new_value: 2012,
    expected_new_result: 2012,
    fun: (function Factory() {
      return function() {
        function A(p) {
          function Inner() {
            return void p;
          }
          debugger;
          return p;
        }
        return A(5);
      };
    })()
  }
];

let salat = 12;
let salad = 12;
export let salami = 1;
export let ham = 1;
import { salami as wurst } from "./debug-modules-set-variable-value.mjs";
import { salami as wurstl } from "./debug-modules-set-variable-value.mjs";

globalThis.testNonExisting = () => {
  let threwBefore = false;
  try { spargel; } catch (e) { if (e instanceof ReferenceError) threwBefore = true; }
  debugger;
  let threwAfter = false;
  try { spargel; } catch (e) { if (e instanceof ReferenceError) threwAfter = true; }
  return threwBefore && threwAfter;
};

globalThis.testSalat = () => {
  const old = salat;
  debugger;
  return [old, salat];
};

globalThis.testSalad = () => {
  function foo() {
    const old = salad;
    debugger;
    return [old, salad];
  }
  return foo();
};

globalThis.testSalami = () => {
  const old = salami;
  debugger;
  return [old, salami];
};

globalThis.testHam = () => {
  function foo() {
    const old = ham;
    debugger;
    return [old, ham];
  }
  return foo();
};

globalThis.testWurst = () => {
  const old = wurst;
  debugger;
  return [old, wurst];
};

globalThis.testWurstl = () => {
  function foo() {
    const old = wurstl;
    debugger;
    return [old, wurstl];
  }
  return foo();
};
`;

(async function test() {
  await Protocol.Debugger.enable();

  async function getScopeProperty(frame, scopeNumber, variableName) {
    const scope = frame.scopeChain[scopeNumber];
    const {result: {result: props}} = await Protocol.Runtime.getProperties({
      objectId: scope.object.objectId,
      ownProperties: true,
    });
    const prop = props.find(p => p.name === variableName);
    return prop ? prop.value.value : undefined;
  }

  let pauseHandler = null;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (pauseHandler) {
      await pauseHandler(callFrames);
    }
    await Protocol.Debugger.resume();
  });

  async function runPauseTest(
      scopeNumber, expectedOldResult, variableName, newValue, expectedNewResult,
      evalExpression) {
    // 1. Initial run without modification.
    pauseHandler = null;
    let res = await Protocol.Runtime.evaluate({expression: evalExpression});
    if (res.result.result.value !== expectedOldResult) {
      InspectorTest.log(
          `Unexpected old result: got ${res.result.result.value}, ` +
          `expected ${expectedOldResult}`);
    }

    // 2. Second run: verify scope property with getProperties, then mutate.
    let verifiedOld = null;
    let verifiedEval = null;
    pauseHandler = async (callFrames) => {
      const frame = callFrames[0];
      verifiedOld = await getScopeProperty(frame, scopeNumber, variableName);

      await Protocol.Debugger.setVariableValue({
        callFrameId: frame.callFrameId,
        scopeNumber,
        variableName,
        newValue: {value: newValue},
      });

      const {result: evalRes} = await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId: frame.callFrameId,
        expression: variableName,
      });
      verifiedEval = evalRes.result.value;
    };

    res = await Protocol.Runtime.evaluate({expression: evalExpression});
    if (res.result.result.value !== expectedNewResult) {
      InspectorTest.log(
          `Unexpected new result: got ${res.result.result.value}, ` +
          `expected ${expectedNewResult}`);
    }
    return {verifiedOld, verifiedEval};
  }

  contextGroup.addModule(source, './debug-modules-set-variable-value.mjs');

  InspectorTest.log('Running closure test cases...');
  for (let i = 0; i < 5; i++) {
    const {result: {result: info}} = await Protocol.Runtime.evaluate({
      expression: `({
        scopeNumber: closure_test_cases[${i}].scope_index_ + 1,
        expectedOldResult: closure_test_cases[${i}].old_result_,
        variableName: closure_test_cases[${i}].variable_name_,
        newValue: closure_test_cases[${i}].new_value_,
        expectedNewResult: closure_test_cases[${i}].new_result_,
      })`,
      returnByValue: true,
    });
    const c = info.value;
    await Protocol.Runtime.evaluate({
      expression:
          `var currentClosureTest = closure_test_cases[${i}].factory_(true);`,
    });
    const {verifiedOld, verifiedEval} = await runPauseTest(
        c.scopeNumber, c.expectedOldResult, c.variableName, c.newValue,
        c.expectedNewResult, 'currentClosureTest();');
    InspectorTest.log(
        `Closure test ${i}: ${c.variableName} (${verifiedOld} -> ` +
        `${verifiedEval}) => ${c.expectedNewResult}`);
  }

  InspectorTest.log('Running local test cases...');
  for (let i = 0; i < 5; i++) {
    const {result: {result: info}} = await Protocol.Runtime.evaluate({
      expression: `({
        scopeNumber: local_test_cases[${i}].scope_number,
        expectedOldResult: local_test_cases[${i}].expected_old_result,
        variableName: local_test_cases[${i}].variable_name,
        newValue: local_test_cases[${i}].new_value,
        expectedNewResult: local_test_cases[${i}].expected_new_result,
      })`,
      returnByValue: true,
    });
    const l = info.value;
    const {verifiedOld, verifiedEval} = await runPauseTest(
        l.scopeNumber, l.expectedOldResult, l.variableName, l.newValue,
        l.expectedNewResult, `local_test_cases[${i}].fun();`);
    InspectorTest.log(
        `Local test ${i}: ${l.variableName} (${verifiedOld} -> ` +
        `${verifiedEval}) => ${l.expectedNewResult}`);
  }

  InspectorTest.log('Running module scope tests...');

  // 1. Non-existing variable
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const moduleScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'module');
    InspectorTest.log(
        `spargel scope: ${frame.scopeChain[moduleScopeIndex].type}`);
    let res = await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: moduleScopeIndex,
      variableName: 'spargel',
      newValue: {value: 42},
    });
    InspectorTest.log(
        `spargel error: ${res.error ? res.error.message : 'none'}`);
  };
  let res = await Protocol.Runtime.evaluate({
    expression: 'testNonExisting()',
    returnByValue: true,
  });
  InspectorTest.log(
      `spargel reference error verified: ${res.result.result.value}`);

  // 2. Local (non-exported) variable
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const moduleScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'module');
    const oldVal = await getScopeProperty(frame, moduleScopeIndex, 'salat');
    await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: moduleScopeIndex,
      variableName: 'salat',
      newValue: {value: 42},
    });
    const {result: evalRes} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: frame.callFrameId,
      expression: 'salat',
    });
    InspectorTest.log(`salat: (${oldVal} -> ${evalRes.result.value})`);
  };
  res = await Protocol.Runtime.evaluate({
    expression: 'testSalat()',
    returnByValue: true,
  });
  InspectorTest.log(`salat result: ${res.result.result.value[1]}`);

  // 3. Local (non-exported) variable, nested access
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const moduleScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'module');
    const oldVal = await getScopeProperty(frame, moduleScopeIndex, 'salad');
    await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: moduleScopeIndex,
      variableName: 'salad',
      newValue: {value: 42},
    });
    const {result: evalRes} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: frame.callFrameId,
      expression: 'salad',
    });
    InspectorTest.log(`salad: (${oldVal} -> ${evalRes.result.value})`);
  };
  res = await Protocol.Runtime.evaluate({
    expression: 'testSalad()',
    returnByValue: true,
  });
  InspectorTest.log(`salad result: ${res.result.result.value[1]}`);

  // 4. Exported variable
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const moduleScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'module');
    const oldVal = await getScopeProperty(frame, moduleScopeIndex, 'salami');
    await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: moduleScopeIndex,
      variableName: 'salami',
      newValue: {value: 2},
    });
    const {result: evalRes} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: frame.callFrameId,
      expression: 'salami',
    });
    InspectorTest.log(`salami: (${oldVal} -> ${evalRes.result.value})`);
  };
  res = await Protocol.Runtime.evaluate({
    expression: 'testSalami()',
    returnByValue: true,
  });
  InspectorTest.log(`salami result: ${res.result.result.value[1]}`);

  // 5. Exported variable, nested access
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const moduleScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'module');
    const oldVal = await getScopeProperty(frame, moduleScopeIndex, 'ham');
    await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: moduleScopeIndex,
      variableName: 'ham',
      newValue: {value: 2},
    });
    const {result: evalRes} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: frame.callFrameId,
      expression: 'ham',
    });
    InspectorTest.log(`ham: (${oldVal} -> ${evalRes.result.value})`);
  };
  res = await Protocol.Runtime.evaluate({
    expression: 'testHam()',
    returnByValue: true,
  });
  InspectorTest.log(`ham result: ${res.result.result.value[1]}`);

  // 6. Imported variable
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const moduleScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'module');
    const oldVal = await getScopeProperty(frame, moduleScopeIndex, 'wurst');
    let res = await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: moduleScopeIndex,
      variableName: 'wurst',
      newValue: {value: 3},
    });
    InspectorTest.log(
        `wurst: ${oldVal}, error: ${res.error ? res.error.message : 'none'}`);
  };
  res = await Protocol.Runtime.evaluate({
    expression: 'testWurst()',
    returnByValue: true,
  });
  InspectorTest.log(`wurst result: ${res.result.result.value[1]}`);

  // 7. Imported variable, nested access
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const moduleScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'module');
    const oldVal = await getScopeProperty(frame, moduleScopeIndex, 'wurstl');
    let res = await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: moduleScopeIndex,
      variableName: 'wurstl',
      newValue: {value: 3},
    });
    InspectorTest.log(
        `wurstl: ${oldVal}, error: ${res.error ? res.error.message : 'none'}`);
  };
  res = await Protocol.Runtime.evaluate({
    expression: 'testWurstl()',
    returnByValue: true,
  });
  InspectorTest.log(`wurstl result: ${res.result.result.value[1]}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
