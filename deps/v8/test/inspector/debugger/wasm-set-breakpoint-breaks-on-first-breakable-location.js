// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests if breakpoint set is first breakable location');

var builder = new WasmModuleBuilder();

// clang-format off
var func_idx = builder.addFunction('helper', kSig_v_v)
    .addLocals(kWasmI32, 1)
    .addBody([
        kExprNop,
        kExprI32Const, 12,
        kExprLocalSet, 0,
    ]).index;

builder.addFunction('main', kSig_v_i)
    .addBody([
        kExprLocalGet, 0,
        kExprIf, kWasmStmt,
        kExprBlock, kWasmStmt,
        kExprCallFunction, func_idx,
        kExprEnd,
        kExprEnd
    ]).exportAs('main');
// clang-format on

var module_bytes = builder.toArray();
Protocol.Debugger.enable();

runTest()
    .catch(reason => InspectorTest.log(`Failed: ${reason}.`))
    .then(InspectorTest.completeTest);

async function runTest() {
  InspectorTest.log('Running test function...');
  WasmInspectorTest.instantiate(module_bytes);
  const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);
  await checkSetBreakpointForScript(wasmScript.scriptId);
  InspectorTest.log('Finished!');
}

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
}

async function checkSetBreakpointForScript(scriptId) {
  // If we try to set a breakpoint that is outside of any function,
  // setBreakpoint should not add any breakpoint.
  InspectorTest.log('Set breakpoint outside of any function: (0, 0).');
  await checkSetBreakpointIsFirstBreakableLocation(scriptId, 0, 0, undefined);

  // If we try to set a breakpoint that is inside of a function and
  // the location is breakable, setBreakpoint is expected to add
  // a breakpoint at that location.
  InspectorTest.log('Set breakpoint at a breakable location: (0, 40).');
  let breakable = true;
  await checkSetBreakpointIsFirstBreakableLocation(scriptId, 0, 40, breakable);

  // If we try to set a breakpoint that is inside of a function and
  // the location is not breakable, setBreakpoint is expected to add
  // a breakpoint at the next breakable location.
  InspectorTest.log('Set breakpoint at non-breakable location: (0, 42).')
  breakable = false;
  await checkSetBreakpointIsFirstBreakableLocation(scriptId, 0, 42, breakable);
}

async function checkSetBreakpointIsFirstBreakableLocation(
    scriptId, lineNumber, columnNumber, breakable) {
  // Choose an arbitrary end column number, as long as a breakable location is
  // contained.
  const endColumnNumber = columnNumber + 10;
  const possibleLocationsMsg = await Protocol.Debugger.getPossibleBreakpoints({
    start: {
      lineNumber: lineNumber,
      columnNumber: columnNumber,
      scriptId: scriptId
    },
    end: {
      lineNumber: lineNumber,
      columnNumber: endColumnNumber,
      scriptId: scriptId
    }
  });
  const possibleLocations = possibleLocationsMsg.result.locations;

  const setLocationMsg =
      await setBreakpoint(scriptId, lineNumber, columnNumber);
  if (!setLocationMsg.result) {
    InspectorTest.log('Set breakpoint could not resolve break location.');
    return;
  }
  var setLocation = setLocationMsg.result.actualLocation;

  // Check that setting a breakpoint at a line actually
  // sets the breakpoint at the first breakable location.
  locationIsEqual(setLocation, possibleLocations[0]);

  // Make sure that the selected locations for the test
  // are breakable/non breakable as expected.
  if (breakable ===
      (setLocation.lineNumber === lineNumber &&
       setLocation.columnNumber === columnNumber)) {
    InspectorTest.log(
        `Initial location is expected to be breakable: ${breakable}.`);
  };
}

function locationIsEqual(locA, locB) {
  if (locA.lineNumber === locB.lineNumber &&
      locA.columnNumber === locB.columnNumber) {
    InspectorTest.log(
        `Location match for (${locA.lineNumber}, ${locA.columnNumber}).`);
  }
}

async function setBreakpoint(id, lineNumber, columnNumber) {
  InspectorTest.log(
      `Setting breakpoint for id: ${id} at ${lineNumber}, ${columnNumber}.`);
  const location = {
    scriptId: id,
    lineNumber: lineNumber,
    columnNumber: columnNumber
  };
  const msg = await Protocol.Debugger.setBreakpoint({location: location});
  printFailure(msg);
  return msg;
}
