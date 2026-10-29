// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests if breakpoint set is first breakable location');

const source = `
// Add a comment to test a break location that is
// outside of a function.
function foo() {
  return Promise.resolve();
}
function boo() {
  return Promise.resolve().then(() => 42);
}`;

contextGroup.addScript(source);
Protocol.Debugger.enable();

runTest()
    .catch(reason => InspectorTest.log(`Failed: ${reason}.`))
    .then(InspectorTest.completeTest);

async function runTest() {
  const {params: script} = await Protocol.Debugger.onceScriptParsed();
  await checkSetBreakpointForScript(script.scriptId);
}

async function checkSetBreakpointForScript(scriptId) {
  // If we try to set a breakpoint that is outside of any function,
  // setBreakpoint will find the first breakable location outside of any
  // functions. If there is none, then it sets a breakpoint at the end of the
  // script.
  InspectorTest.log('Set breakpoint outside of any function: (0, 0).');
  await checkSetBreakpointIsFirstBreakableLocation(scriptId, 0, 0, undefined);

  // If we try to set a breakpoint that is inside of a function and
  // the location is breakable, setBreakpoint is expected to add
  // a breakpoint at that location.
  InspectorTest.log('Set breakpoint at a breakable location: (4, 17).');
  let breakable = true;
  await checkSetBreakpointIsFirstBreakableLocation(scriptId, 4, 17, breakable);

  // If we try to set a breakpoint that is inside of a function and
  // the location is not breakable, setBreakpoint is expected to add
  // a breakpoint at the next breakable location.
  InspectorTest.log('Set breakpoint at non-breakable location: (7, 0).')
  breakable = false;
  await checkSetBreakpointIsFirstBreakableLocation(scriptId, 7, 0, breakable);
}

async function checkSetBreakpointIsFirstBreakableLocation(
    scriptId, lineNumber, columnNumber, breakable) {
  const possibleLocationsMsg = await Protocol.Debugger.getPossibleBreakpoints({
    start: {
      lineNumber: lineNumber,
      columnNumber: columnNumber,
      scriptId: scriptId
    },
    end: {
      lineNumber: lineNumber + 1,
      columnNumber: columnNumber,
      scriptId: scriptId
    }
  });
  const setLocationMsg =
      await setBreakpoint(scriptId, lineNumber, columnNumber);
  const setLocation = setLocationMsg.result.actualLocation;

  if (possibleLocationsMsg.result.locations.length === 0) {
    InspectorTest.log('No breakable location inside a function was found');
    InspectorTest.log(`Set breakpoint adds a breakpoint at (${
        setLocation.lineNumber}, ${setLocation.columnNumber}).`);
    return;
  }
  const possibleLocations = possibleLocationsMsg.result.locations;

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
  return msg;
}
