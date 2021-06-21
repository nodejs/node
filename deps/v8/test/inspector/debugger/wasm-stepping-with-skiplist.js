// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through wasm scripts by byte offsets');
session.setupScriptMap();

const builder = new WasmModuleBuilder();

const func_a =
    builder.addFunction('wasm_A', kSig_v_i).addBody([kExprNop, kExprNop]);
const func_a_idx = func_a.index;

// wasm_B calls wasm_A <param0> times.
const func_b = builder.addFunction('wasm_B', kSig_v_i)
    .addBody([
      // clang-format off
      kExprLoop, kWasmVoid,               // while
        kExprLocalGet, 0,                 // -
        kExprIf, kWasmVoid,               // if <param0> != 0
          kExprLocalGet, 0,               // -
          kExprI32Const, 1,               // -
          kExprI32Sub,                    // -
          kExprLocalSet, 0,               // decrease <param0>
          ...wasmI32Const(1024),          // some longer i32 const (2 byte imm)
          kExprCallFunction, func_a_idx,  // -
          kExprBr, 1,                     // continue
          kExprEnd,                       // -
        kExprEnd,                         // break
      // clang-format on
    ])
    .exportAs('main');

const module_bytes = builder.toArray();
const loop_start_offset = func_b.body_offset + 2;
const loop_body_start_offset = loop_start_offset + 2;
const loop_body_end_offset = loop_body_start_offset + 14;
const if_statement_offset = loop_body_start_offset + 2
const call_function_offset = loop_body_start_offset + 12;

const func_a_start_offset = func_a.body_offset;
const func_a_end_offset = func_a_start_offset + 2;

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Debugger.enable();
    InspectorTest.log('Setting up global instance variable');
    WasmInspectorTest.instantiate(module_bytes);
    const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);
    const scriptId = wasmScript.scriptId;

    InspectorTest.log('Got wasm script: ' + wasmScript.url);

    let bpmsg = await Protocol.Debugger.setBreakpoint({
      location:
          {scriptId: scriptId, lineNumber: 0, columnNumber: loop_start_offset}
    });
    InspectorTest.logMessage(bpmsg.result.actualLocation);

    await checkValidSkipLists(scriptId);
    await checkInvalidSkipLists(scriptId);
  }
]);

async function checkValidSkipLists(scriptId) {
  InspectorTest.log('Test with valid skip lists');
  Protocol.Runtime.evaluate({expression: 'instance.exports.main(8)'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(callFrames[0].location);

  InspectorTest.log('Test: Stepping over without skip list');
  let skipList = [];
  await stepThroughOneLoopIteration(skipList, 'stepOver');

  InspectorTest.log('Test: Stepping over with skip list');
  skipList = [
    createLocationRange(scriptId, loop_body_start_offset, if_statement_offset),
    createLocationRange(scriptId, call_function_offset, loop_body_end_offset)
  ];
  await stepThroughOneLoopIteration(skipList, 'stepOver');

  InspectorTest.log('Test: Stepping over start location is inclusive');
  skipList = [
    createLocationRange(
        scriptId, loop_body_start_offset, loop_body_end_offset - 1),
  ];
  await stepThroughOneLoopIteration(skipList, 'stepOver');

  InspectorTest.log('Test: Stepping over end location is exclusive');
  skipList = [
    createLocationRange(
        scriptId, loop_body_start_offset + 1, loop_body_end_offset),
  ];
  await stepThroughOneLoopIteration(skipList, 'stepOver');

  InspectorTest.log('Test: Stepping into without skip list');
  skipList = [];
  await stepThroughOneLoopIteration(skipList, 'stepInto');

  InspectorTest.log(
      'Test: Stepping into with skip list, while call itself is skipped');
  skipList = [
    createLocationRange(scriptId, func_a_start_offset, func_a_end_offset),
    createLocationRange(scriptId, if_statement_offset, loop_body_end_offset)
  ];
  await stepThroughOneLoopIteration(skipList, 'stepInto');

  InspectorTest.log('Test: Stepping into start location is inclusive');
  skipList = [
    createLocationRange(scriptId, func_a_start_offset, func_a_end_offset - 1),
  ];
  await stepThroughOneLoopIteration(skipList, 'stepInto');

  InspectorTest.log('Test: Stepping into end location is exclusive');
  skipList = [
    createLocationRange(scriptId, func_a_start_offset - 1, func_a_end_offset),
  ];
  await stepThroughOneLoopIteration(skipList, 'stepInto');

  await Protocol.Debugger.resume();
}

async function checkInvalidSkipLists(scriptId) {
  InspectorTest.log('Test with invalid skip lists');
  Protocol.Runtime.evaluate({expression: 'instance.exports.main(8)'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(callFrames[0].location);

  const actions = ['stepOver', 'stepInto'];
  for (let action of actions) {
    InspectorTest.log('Test: start position has invalid column number');
    let skipList = [
      createLocationRange(scriptId, -1, loop_body_end_offset),
    ];
    await stepThroughOneLoopIteration(skipList, action);

    InspectorTest.log('Test: start position has invalid line number');
    skipList = [{
      scriptId: scriptId,
      start: {lineNumber: -1, columnNumber: 0},
      end: {lineNumber: 0, columnNumber: loop_body_end_offset}
    }];
    await stepThroughOneLoopIteration(skipList, action);

    InspectorTest.log('Test: end position smaller than start position');
    skipList = [createLocationRange(
        scriptId, loop_body_end_offset, loop_body_start_offset)];
    await stepThroughOneLoopIteration(skipList, action);

    InspectorTest.log('Test: skip list is not maximally merged');
    skipList = [
      createLocationRange(
          scriptId, loop_body_start_offset, if_statement_offset),
      createLocationRange(scriptId, if_statement_offset, loop_body_end_offset)
    ];
    await stepThroughOneLoopIteration(skipList, action);

    InspectorTest.log('Test: skip list is not sorted');
    skipList = [
      createLocationRange(scriptId, if_statement_offset, loop_body_end_offset),
      createLocationRange(
          scriptId, loop_body_start_offset, loop_body_end_offset)
    ];
    await stepThroughOneLoopIteration(skipList, action);
  }
}

async function stepThroughOneLoopIteration(skipList, stepAction) {
  InspectorTest.log(
      `Testing ${stepAction} with skipList: ${JSON.stringify(skipList)}`);
  let topFrameLocation = -1;
  while (topFrameLocation.columnNumber != loop_start_offset) {
    const stepOverMsg = await Protocol.Debugger[stepAction]({skipList});
    if (stepOverMsg.error) {
      InspectorTest.log(stepOverMsg.error.message);
      return;
    }
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    topFrameLocation = callFrames[0].location;
    await session.logSourceLocation(topFrameLocation);
  }
}

function createLocationRange(scriptId, startColumn, endColumn) {
  return {
    scriptId: scriptId, start: {lineNumber: 0, columnNumber: startColumn},
        end: {lineNumber: 0, columnNumber: endColumn}
  }
}
