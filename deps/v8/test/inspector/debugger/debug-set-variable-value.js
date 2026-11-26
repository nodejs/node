// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Debugger.setVariableValue across different scope types.');

contextGroup.addScript(
    `
let abc = 12;

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

var closure_test_cases = [
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

  new ClosureTestCase(0, 17, 's', 'Bird', 'Bird', true,
      function Factory(debug_stop) {
    eval("var s = 17");
    return function() {
      if (debug_stop) debugger;
      return s;
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

var local_test_cases = [
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
    expected_old_result: 'mouse',
    variable_name: 'v1',
    new_value: 'dog',
    expected_new_result: 'dog',
    fun: (function Factory() {
      return function() {
        eval("var v1 = 'mouse'");
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
`,
    0, 0, 'test.js');

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

  InspectorTest.log('Running closure test cases...');
  for (let i = 0; i < 6; i++) {
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
  for (let i = 0; i < 6; i++) {
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

  InspectorTest.log('Running script scope test...');
  let scriptVerifiedOld = null;
  let scriptVerifiedEval = null;
  pauseHandler = async (callFrames) => {
    const frame = callFrames[0];
    const scriptScopeIndex =
        frame.scopeChain.findIndex(s => s.type === 'script');
    if (scriptScopeIndex === -1) {
      InspectorTest.log('Script scope not found');
      return;
    }
    scriptVerifiedOld = await getScopeProperty(frame, scriptScopeIndex, 'abc');
    await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber: scriptScopeIndex,
      variableName: 'abc',
      newValue: {value: 42},
    });
    const {result: evalRes} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: frame.callFrameId,
      expression: 'abc',
    });
    scriptVerifiedEval = evalRes.result.value;
  };

  let res = await Protocol.Runtime.evaluate({
    expression: '(() => { const old = abc; debugger; return [old, abc]; })();',
    returnByValue: true,
  });
  InspectorTest.log(
      `Script test: abc (${scriptVerifiedOld} -> ${scriptVerifiedEval}) => ` +
      `${res.result.result.value[1]}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
