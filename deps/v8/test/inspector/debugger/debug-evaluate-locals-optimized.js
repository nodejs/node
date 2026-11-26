// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests evaluating locals and arguments on optimized inlined frames.');

const expected = [
  {
    locals: {i: 0, x0: 3, y0: 4, a0: 1, b0: 2},
    args: {names: ['i', 'x0', 'y0'], values: [0, 3, 4]}
  },
  {
    locals: {i: 1, x1: 5, y1: 6, a1: 3, b1: 4},
    args: {names: ['i', 'x1', 'y1'], values: [1, 5, 6]}
  },
  {locals: {i: 2, a2: 5, b2: 6}, args: {names: ['i'], values: [2]}}, {
    locals: {i: 3, x3: 9, y3: 10, z3: undefined, a3: 7, b3: 8},
    args: {names: ['i', 'x3', 'y3', 'z3'], values: [3, 9, 10, undefined]}
  },
  {
    locals: {i: 4, x4: 11, y4: 12, a4: 9, b4: 10},
    args: {names: ['i', 'x4', 'y4'], values: [4, 11, 12]}
  }
];

function propertiesToObject(props) {
  const obj = {};
  props.forEach(elem => {
    const key = elem.name;
    let value;
    if (elem.value) {
      switch (elem.value.type) {
        case 'undefined':
          value = undefined;
          break;
        default:
          value = elem.value.value;
          break;
      }
    }
    obj[key] = value;
  });
  return obj;
}

async function getProperties(objectId) {
  const {result: {result}} =
      await Protocol.Runtime.getProperties({objectId, ownProperties: true});
  return result;
}

async function getLocalScopeDetails(frame) {
  const scopes = frame.scopeChain;
  for (let i = 0; i < scopes.length; i++) {
    const scope = scopes[i];
    if (scope.type == 'local') {
      return await getProperties(scope.object.objectId);
    }
  }
  return undefined;
}

async function localCount(frame) {
  const scopeDetails = await getLocalScopeDetails(frame);
  return scopeDetails ? scopeDetails.length : 0;
}

async function localName(frame, index) {
  const scopeDetails = await getLocalScopeDetails(frame);
  if (index < 0 || index >= scopeDetails.length) return undefined;
  return scopeDetails[index].name;
}

async function localValue(frame, index) {
  const scopeDetails = await getLocalScopeDetails(frame);
  if (index < 0 || index >= scopeDetails.length) return undefined;
  const local = scopeDetails[index];
  if (local.value === undefined) return {isUnavailable: () => true};
  let val;
  switch (local.value.type) {
    case 'undefined':
      val = undefined;
      break;
    default:
      val = local.value.value;
      break;
  }
  return {value: () => val, isUnavailable: () => false};
}

function execStateScope(frame, scopeIndex) {
  const scope = frame.scopeChain[scopeIndex];
  return {
    scopeType: () => scope.type,
    scopeObject: async () => {
      const serializedScope = await getProperties(scope.object.objectId);
      const scopeObj = propertiesToObject(serializedScope);
      return {value: () => scopeObj};
    }
  };
}

function deepObjectEquals(a, b) {
  const aProps = Object.getOwnPropertyNames(a);
  aProps.sort();
  const bProps = Object.getOwnPropertyNames(b);
  bProps.sort();
  if (aProps.length !== bProps.length) return false;
  for (let i = 0; i < aProps.length; i++) {
    if (aProps[i] !== bProps[i]) return false;
    if (a[aProps[i]] !== b[bProps[i]]) return false;
  }
  return true;
}

(async function test() {
  await Protocol.Debugger.enable();

  let totalPauseCount = 0;
  let verifiedPauseCount = 0;
  let resolveDone;
  const donePromise = new Promise(resolve => {
    resolveDone = resolve;
  });

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    totalPauseCount++;
    if (totalPauseCount <= 5) {
      await Protocol.Debugger.resume();
      return;
    }
    verifiedPauseCount++;
    const checkFrameCount = 6;
    if (callFrames.length !== checkFrameCount) {
      InspectorTest.log(`Unexpected frame count: ${callFrames.length}`);
    }
    for (let i = 0; i < checkFrameCount; i++) {
      const frame = callFrames[i];
      if (i < checkFrameCount - 1) {
        const expectedArgs = expected[i].args;
        const expectedLocals = expected[i].locals;

        const locals = {};
        for (let j = 0; j < await localCount(frame); j++) {
          locals[await localName(frame, j)] =
              (await localValue(frame, j)).value();
        }
        if (!deepObjectEquals(expectedLocals, locals)) {
          InspectorTest.log(`Mismatch locals on frame ${i}`);
        }

        if (frame.scopeChain.length !== 3 ||
            execStateScope(frame, 0).scopeType() !== 'local' ||
            execStateScope(frame, 1).scopeType() !== 'script' ||
            execStateScope(frame, 2).scopeType() !== 'global') {
          InspectorTest.log(`Unexpected scopes on frame ${i}`);
        }

        for (const name of Object.keys(expectedLocals)) {
          const val =
              (await execStateScope(frame, 0).scopeObject()).value()[name];
          if (val !== expectedLocals[name]) {
            InspectorTest.log(
                `Mismatch scopeObject local ${name} on frame ${i}`);
          }
        }

        for (let j = 0; j < expectedArgs.names.length; j++) {
          const argName = expectedArgs.names[j];
          const argValue = expectedArgs.values[j];
          const val =
              (await execStateScope(frame, 0).scopeObject()).value()[argName];
          if (val !== argValue) {
            InspectorTest.log(`Mismatch scope arg ${argName} on frame ${i}`);
          }
        }

        for (const name of Object.keys(expectedLocals)) {
          const {result: {result}} =
              await Protocol.Debugger.evaluateOnCallFrame(
                  {callFrameId: frame.callFrameId, expression: name});
          const val = result.type === 'undefined' ? undefined : result.value;
          if (val !== expectedLocals[name]) {
            InspectorTest.log(`Mismatch eval local ${name} on frame ${i}`);
          }
        }

        for (let j = 0; j < expectedArgs.names.length; j++) {
          const argName = expectedArgs.names[j];
          const argValue = expectedArgs.values[j];
          let {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: frame.callFrameId, expression: argName});
          let val = result.type === 'undefined' ? undefined : result.value;
          if (val !== argValue) {
            InspectorTest.log(`Mismatch eval arg ${argName} on frame ${i}`);
          }

          ({result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
            callFrameId: frame.callFrameId,
            expression: 'arguments[' + j + ']'
          }));
          val = result.type === 'undefined' ? undefined : result.value;
          if (val !== argValue) {
            InspectorTest.log(`Mismatch eval arguments[${j}] on frame ${i}`);
          }
        }
      } else {
        if (frame.scopeChain.length !== 2 ||
            execStateScope(frame, 0).scopeType() !== 'script' ||
            execStateScope(frame, 1).scopeType() !== 'global') {
          InspectorTest.log(`Unexpected scopes on bottom frame`);
        }
      }

      switch (i) {
        case 0:
          if (frame.functionName !== 'h') InspectorTest.log('bad fn 0');
          break;
        case 1:
          if (frame.functionName !== 'g3') InspectorTest.log('bad fn 1');
          break;
        case 2:
          if (frame.functionName !== 'g2') InspectorTest.log('bad fn 2');
          break;
        case 3:
          if (frame.functionName !== 'g1') InspectorTest.log('bad fn 3');
          break;
        case 4:
          if (frame.functionName !== 'f') InspectorTest.log('bad fn 4');
          break;
        case 5:
          break;
        default:
          InspectorTest.log('unreachable frame ' + i);
      }
    }
    InspectorTest.log(`Pause ${verifiedPauseCount}: verified all frames`);
    await Protocol.Debugger.resume();
    if (verifiedPauseCount === 4) {
      resolveDone();
    }
  });

  contextGroup.addScript(
      `
let testingConstructCall = false;

var expected = [
  { locals: {i: 0, x0: 3, y0: 4, a0: 1, b0: 2},
    args: { names: ["i", "x0", "y0"], values: [0, 3, 4] } },
  { locals: {i: 1, x1: 5, y1: 6, a1: 3, b1: 4},
    args: { names: ["i", "x1", "y1"], values: [1, 5, 6] } },
  { locals: {i: 2, a2: 5, b2: 6},
    args: { names: ["i"], values: [2] } },
  { locals: {i: 3, x3: 9, y3: 10, z3: undefined, a3: 7, b3: 8},
    args: { names: ["i", "x3", "y3", "z3"], values: [3, 9, 10, undefined] } },
  { locals: {i: 4, x4: 11, y4: 12, a4: 9, b4: 10},
    args: { names: ["i", "x4", "y4"], values: [4, 11, 12] } }
];

%PrepareFunctionForOptimization(f);
for (var i = 0; i < 4; i++) f(expected.length - 1, 11, 12);
%OptimizeFunctionOnNextCall(f);
f(expected.length - 1, 11, 12);

function h(i, x0, y0) {
  var a0 = expected[i].locals.a0;
  var b0 = expected[i].locals.b0;
  debugger;  // Breakpoint.
  return a0 + b0;
}

function g3(i, x1, y1) {
  var a1 = expected[i].locals.a1;
  var b1 = expected[i].locals.b1;
  h(i - 1, a1, b1);
  return a1 + b1;
}

function g2(i) {
  var a2 = expected[i].locals.a2;
  var b2 = expected[i].locals.b2;
  g3(i - 1, a2, b2);
  return a2 + b2;
}

function g1(i, x3, y3, z3) {
  var a3 = expected[i].locals.a3;
  var b3 = expected[i].locals.b3;
  new g2(i - 1, a3, b3);
  return a3 + b3;
}

function f(i, x4, y4) {
  var a4 = expected[i].locals.a4;
  var b4 = expected[i].locals.b4;
  g1(i - 1, a4, b4);
  return a4 + b4;
}

f(expected.length - 1, 11, 12);
f(expected.length - 1, 11, 12, 0);
testingConstructCall = true;
new f(expected.length - 1, 11, 12);
new f(expected.length - 1, 11, 12, 0);
gc();
`,
      0, 0, 'test.js');

  await donePromise;
  gc();
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
