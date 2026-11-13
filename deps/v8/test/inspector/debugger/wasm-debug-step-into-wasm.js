// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into Wasm from JavaScript.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

const source = `
var builder = new WasmModuleBuilder();
builder.addFunction('sub', kSig_i_ii)
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI32Sub
  ])
  .exportFunc();
const instance = builder.instantiate();
const wasm_f = instance.exports.sub;

function f() {
  var result = wasm_f(3, 2); // Line 0.
  result++; // Line 1.
  return result; // Line 2.
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  let phase = 1;
  let js_break_line = 0;
  let break_count = 0;
  let wasm_break_count = 0;
  let break_count2 = 0;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const topFrame = callFrames[0];
    const fnName = topFrame.functionName;
    if (fnName !== 'f' && fnName !== '$sub') {
      await Protocol.Debugger.resume();
      return;
    }
    InspectorTest.log(fnName);
    await session.logSourceLocation(topFrame.location);
    if (phase === 1) {
      if (fnName === 'f') {
        break_count++;
        js_break_line++;
      } else {
        wasm_break_count++;
      }
      await Protocol.Debugger.stepInto();
    } else {
      break_count2++;
      await Protocol.Debugger.stepOut();
    }
  });

  const lines = source.split('\n');
  const line0 = lines.findIndex(l => l.includes('Line 0.'));
  const line2 = lines.findIndex(l => l.includes('Line 2.'));

  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: line0,
  });
  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: line2,
  });

  InspectorTest.log('Executing f() with stepInto');
  await Protocol.Runtime.evaluate({expression: 'f()'});

  phase = 2;
  InspectorTest.log('Executing f() with stepOut');
  await Protocol.Runtime.evaluate({expression: 'f()'});

  InspectorTest.log(`assertEquals(3, break_count): ${break_count === 3}`);
  InspectorTest.log(`assertEquals(3, js_break_line): ${js_break_line === 3}`);
  InspectorTest.log(
      `assertEquals(4, wasm_break_count): ${wasm_break_count === 4}`);
  InspectorTest.log(`assertEquals(2, break_count2): ${break_count2 === 2}`);

  InspectorTest.completeTest();
})();
