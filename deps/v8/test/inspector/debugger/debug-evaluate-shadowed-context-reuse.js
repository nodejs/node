// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness
// Flags: --experimental-reuse-locals-blocklists

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests debug-evaluate variable resolution with shadowed context ' +
    'variables and blocklist reuse.');

contextGroup.addScript(`
var a = "a";
function f1() {
  var x = 1;     // context allocate x
  (() => x);
  var y = "y";
  var z = "z";
  (function () {
    var x = 2;   // stack allocate shadowing x
    (function () {
      y;         // access y
      debugger;  // ReferenceError
    })();        // 2
  })();          // 1
  return y;
}

function f2() {
  var x = 1;     // context allocate x
  (() => x);
  var y = "y";
  var z = "z";
  (function() {
    var x = 2;   // stack allocate shadowing x
    (() => {
      y;
      a;
      this;      // context allocate receiver
      debugger;  // ReferenceError
    })();        // 2
  })();          // 1
  return y;
}

function f3() {
  var x = 1;     // context allocate x
  (() => x);
  (function() {
    var x = 2;   // stack allocate shadowing x
    (function() {
      {          // context allocate x in a nested scope
        let x = 3;
        (() => x);
      }
      debugger;
    })();
  })();
}

function f4() {
  let a = 1;
  let b = 2;
  let c = 3;
  () => a + c;  // a and c are context-allocated
  return function g() {
    let a = 2;  // a is stack-allocated
    return function h() {
      b;  // b is allocated onto f's context.
      debugger;
    }
  }
}

function f5() {
  let a = 1;
  () => a;  // a is context-allocated
  return function g() {
    let a = 2;  // a is stack-allocated
    {
      let b = 3;
      return function h() {
        debugger;
      }
    }
  }
}

function test_scopes_shadowing() {
  let x = 'outer_x';
  let y = 'outer_y';
  () => [x, y];  // context allocate x and y in f7
  return function g() {
    let x = 'middle_x';
    () => x;  // context allocate x in g
    return function h() {
      let x = 'inner_x_stack';  // stack allocate x in h
      debugger;
    };
  };
}

let break_position;
function f6() {
  let a = 1;                             // stack-allocated
  return function g() {                  // g itself doesn't require a context.
    if (break_position === 2) debugger;
    let a = 2; (() => a);                // context-allocated
    return function h() {
      if (break_position === 1) debugger;
    }
  }
}

function f7(o) {
  let a = 1;  // stack-allocated.
  with (o) {  // create a with-scope whose block-list has 'a' in it.
    if (break_position === 2) debugger;
    (function g() {
      // Trigger block-list calculation for the with-scope.
      if (break_position === 1) debugger;
    })();
  }
}

var x = 'global_x';
function test_outer_shadow() {
  let x = 'outer_stack';
  let closure_y = 'closure_y';
  () => closure_y; // force context allocation for closure_y
  return function inner() {
    debugger;
  };
}

function test_outer_shadow_with_inner_context() {
  let x = 'outer_stack';
  let closure_y = 'closure_y';
  // force context allocation for test_outer_shadow_with_inner_context
  () => closure_y;
  return function inner() {
    let inner_ctx = 'inner_ctx';
    () => inner_ctx; // force context allocation for inner as well
    debugger;
  };
}

var a_block_test = 'global_a';
var c_block_test = 'global_c';
function test_inner_block_isolation() {
  let a_block_test = 42;  // stack-allocated in f
  let b_block_test = 21;
  () => b_block_test;     // force context-allocation so f has a context
  {
    const c_block_test = 'foo';  // stack-allocated in inner block
    (function g() {
      if (break_position === 1) debugger;
    })();
  }
  return function h() {
    if (break_position === 2) debugger;
  };
}
`);

async function evalOnFrame(frame, expression, scopeNumber = undefined) {
  const params = {callFrameId: frame.callFrameId, expression};
  if (scopeNumber !== undefined) {
    params.scopeNumber = scopeNumber;
  }
  const {result} = await Protocol.Debugger.evaluateOnCallFrame(params);
  if (result.exceptionDetails) {
    return result.result.className;
  }
  if (result.result.type === 'object' && result.result.className === 'global') {
    return '[object global]';
  }
  return result.result.value;
}

(async function test() {
  await Protocol.Debugger.enable();

  let currentDelegate = null;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    await currentDelegate(callFrames);
    await Protocol.Debugger.resume();
  });

  async function checkF1OrF2(callFrames) {
    const out = [];
    for (let i = 0; i < callFrames.length - 1; i++) {
      out.push(await evalOnFrame(callFrames[i], 'x'));
    }
    InspectorTest.log('frames x: ' + JSON.stringify(out));
    InspectorTest.log('this: ' + await evalOnFrame(callFrames[0], 'this'));
    InspectorTest.log('y: ' + await evalOnFrame(callFrames[0], 'y'));
    InspectorTest.log('a: ' + await evalOnFrame(callFrames[0], 'a'));
    await evalOnFrame(callFrames[0], 'a = \'A\'');
    InspectorTest.log('z: ' + await evalOnFrame(callFrames[0], 'z'));
  }

  InspectorTest.log('Test f1');
  currentDelegate = checkF1OrF2;
  await Protocol.Runtime.evaluate({expression: 'f1();'});
  let aRes = await Protocol.Runtime.evaluate(
      {expression: 'var res = a; a = "a"; res;'});
  InspectorTest.log('after f1 a: ' + aRes.result.result.value);

  InspectorTest.log('Test f2');
  currentDelegate = checkF1OrF2;
  await Protocol.Runtime.evaluate({expression: 'f2();'});
  aRes = await Protocol.Runtime.evaluate({expression: 'a'});
  InspectorTest.log('after f2 a: ' + aRes.result.result.value);

  InspectorTest.log('Test f3');
  currentDelegate = async (callFrames) => {
    InspectorTest.log('f3 x: ' + await evalOnFrame(callFrames[0], 'x'));
  };
  await Protocol.Runtime.evaluate({expression: 'f3();'});

  InspectorTest.log('Test f4');
  currentDelegate = async (callFrames) => {
    InspectorTest.log('f4 b: ' + await evalOnFrame(callFrames[0], 'b'));
    InspectorTest.log('f4 c: ' + await evalOnFrame(callFrames[0], 'c'));
    InspectorTest.log('f4 a: ' + await evalOnFrame(callFrames[0], 'a'));
  };
  await Protocol.Runtime.evaluate({expression: '(f4())()();'});

  InspectorTest.log('Test f5');
  currentDelegate = async (callFrames) => {
    InspectorTest.log('f5 a: ' + await evalOnFrame(callFrames[0], 'a'));
  };
  await Protocol.Runtime.evaluate({expression: '(f5())()();'});

  InspectorTest.log('Test test_scopes_shadowing');
  currentDelegate = async (callFrames) => {
    for (const s of [0, 1, 2]) {
      InspectorTest.log(
          'scope ' + s + ': x=' + await evalOnFrame(callFrames[0], 'x', s) +
          ', y=' + await evalOnFrame(callFrames[0], 'y', s));
    }
  };
  await Protocol.Runtime.evaluate(
      {expression: '(test_scopes_shadowing())()();'});

  InspectorTest.log('Test f6 break_position=1 and 2');
  currentDelegate = async (callFrames) => {
    InspectorTest.log('f6 a: ' + await evalOnFrame(callFrames[0], 'a'));
  };
  await Protocol.Runtime.evaluate(
      {expression: 'break_position = 1; (f6())()();'});
  await Protocol.Runtime.evaluate(
      {expression: 'break_position = 2; (f6())()();'});

  InspectorTest.log('Test f7 break_position=1 and 2');
  currentDelegate = async (callFrames) => {
    InspectorTest.log('f7 a: ' + await evalOnFrame(callFrames[0], 'a'));
  };
  await Protocol.Runtime.evaluate({expression: 'break_position = 1; f7({});'});
  await Protocol.Runtime.evaluate({expression: 'break_position = 2; f7({});'});

  InspectorTest.log('Test test_outer_shadow');
  currentDelegate = async (callFrames) => {
    InspectorTest.log(
        'outer_shadow x: ' + await evalOnFrame(callFrames[0], 'x', 1));
    InspectorTest.log(
        'outer_shadow closure_y: ' +
        await evalOnFrame(callFrames[0], 'closure_y', 1));
  };
  await Protocol.Runtime.evaluate({expression: '(test_outer_shadow())();'});

  InspectorTest.log('Test test_outer_shadow_with_inner_context');
  await Protocol.Runtime.evaluate(
      {expression: '(test_outer_shadow_with_inner_context())();'});

  InspectorTest.log('Test test_inner_block_isolation break_position=1');
  currentDelegate = async (callFrames) => {
    for (const s of [0, 1, 2]) {
      InspectorTest.log(
          'scope ' + s +
          ': a=' + await evalOnFrame(callFrames[0], 'a_block_test', s) +
          ', c=' + await evalOnFrame(callFrames[0], 'c_block_test', s) +
          ', b=' + await evalOnFrame(callFrames[0], 'b_block_test', s));
    }
  };
  await Protocol.Runtime.evaluate(
      {expression: 'break_position = 1; (test_inner_block_isolation())();'});

  InspectorTest.log('Test test_inner_block_isolation break_position=2');
  currentDelegate = async (callFrames) => {
    InspectorTest.log(
        'scope 0: a=' + await evalOnFrame(callFrames[0], 'a_block_test', 0) +
        ', c=' + await evalOnFrame(callFrames[0], 'c_block_test', 0) +
        ', b=' + await evalOnFrame(callFrames[0], 'b_block_test', 0));
  };
  await Protocol.Runtime.evaluate(
      {expression: 'break_position = 2; (test_inner_block_isolation())();'});

  InspectorTest.completeTest();
})();
