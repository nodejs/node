// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests evaluation of locals in call frames.');

contextGroup.addScript(`
function h() {
  var a = 1;
  var b = 2;
  var eval = 5;  // Overriding eval should not break anything.
  debugger;  // Breakpoint.
  return a;
}

function g() {
  var a = 3;
  eval("var b = 4;");
  return h() + a;
}

function f() {
  var a = 5;
  var b = 0;
  with ({b:6}) {
    return g();
  }
}
`);

async function getLocalScopeProperties(frame) {
  for (const scope of frame.scopeChain) {
    if (scope.type === 'local') {
      const {result: {result}} = await Protocol.Runtime.getProperties({
        objectId: scope.object.objectId,
        ownProperties: true,
      });
      return result;
    }
  }
  return [];
}

async function evalOnFrame(frame, expression, scopeNumber = undefined) {
  const params = {callFrameId: frame.callFrameId, expression};
  if (scopeNumber !== undefined) {
    params.scopeNumber = scopeNumber;
  }
  const {result} = await Protocol.Debugger.evaluateOnCallFrame(params);
  if (result.exceptionDetails) {
    return 'throws ' + result.result.className;
  }
  return result.result.value;
}

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const locals0 = await getLocalScopeProperties(callFrames[0]);
    InspectorTest.log('frame 0 local count: ' + locals0.length);
    for (const prop of locals0) {
      InspectorTest.log(
          'frame 0 local ' + prop.name + ' = ' + prop.value.value);
    }

    const locals1 = await getLocalScopeProperties(callFrames[1]);
    InspectorTest.log('frame 1 local count: ' + locals1.length);
    for (const prop of locals1) {
      if (prop.name === 'arguments') {
        InspectorTest.log('frame 1 local arguments');
      } else {
        InspectorTest.log(
            'frame 1 local ' + prop.name + ' = ' + prop.value.value);
      }
    }

    const locals2 = await getLocalScopeProperties(callFrames[2]);
    InspectorTest.log('frame 2 local count: ' + locals2.length);
    for (const prop of locals2) {
      InspectorTest.log(
          'frame 2 local ' + prop.name + ' = ' + prop.value.value);
    }

    InspectorTest.log(
        'frame(0).evaluate("a"): ' + await evalOnFrame(callFrames[0], 'a'));
    InspectorTest.log(
        'frame(0).evaluate("b"): ' + await evalOnFrame(callFrames[0], 'b'));
    InspectorTest.log(
        'frame(0).evaluate("eval"): ' +
        await evalOnFrame(callFrames[0], 'eval'));
    InspectorTest.log(
        'frame(1).evaluate("a"): ' + await evalOnFrame(callFrames[1], 'a'));
    InspectorTest.log(
        'frame(1).evaluate("b"): ' + await evalOnFrame(callFrames[1], 'b'));
    InspectorTest.log(
        'frame(1).evaluate("typeof eval"): ' +
        await evalOnFrame(callFrames[1], 'typeof eval'));
    InspectorTest.log(
        'frame(2).evaluate("a"): ' + await evalOnFrame(callFrames[2], 'a'));
    InspectorTest.log(
        'frame(2).evaluate("b"): ' + await evalOnFrame(callFrames[2], 'b'));
    InspectorTest.log(
        'frame(2).evaluate("typeof eval"): ' +
        await evalOnFrame(callFrames[2], 'typeof eval'));
    InspectorTest.log(
        'frame(0).scope(0).evaluate("a"): ' +
        await evalOnFrame(callFrames[0], 'a', 0));
    InspectorTest.log(
        'frame(0).scope(1).evaluate("a"): ' +
        await evalOnFrame(callFrames[0], 'a', 1));
    InspectorTest.log(
        'frame(0).evaluate(\'a = "foo"\'): ' +
        await evalOnFrame(callFrames[0], 'a = "foo"'));
    InspectorTest.log(
        'frame(1).evaluate(\'a = "bar"\'): ' +
        await evalOnFrame(callFrames[1], 'a = "bar"'));

    await Protocol.Debugger.resume();
  });

  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: 'f()',
    returnByValue: true,
  });
  InspectorTest.log('f() = ' + result.value);
  InspectorTest.completeTest();
})();
