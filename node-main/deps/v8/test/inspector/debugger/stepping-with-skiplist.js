// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that stepOver and stepInto correctly handle skipLists.');

function test(input) {
  debugger;
  var a = 4;
  var sum = 0;
  if (input > 0) {
    sum = a + input;
  }
  var b = 5;
  sum = add(sum, b);
  return sum;
}

function add(a, b) {
  var res = a + b;
  return res;
}

contextGroup.addScript(`${test} //# sourceURL=test.js`);
contextGroup.addScript(`${add}`);

const first_non_debug_line_offset = 2;
const last_line_line_offset = 9;
const function_call_line_offset = 8;
const function_call_column_offset = 2;
const if_case_line_offset = 4;

const add_first_line_line_offset = 1;
const add_last_line_line_offset = 2;
const add_last_line_column_offset = 13;

Protocol.Debugger.enable();
runTest()
    .catch(reason => InspectorTest.log(`Failed: ${reason}`))
    .then(InspectorTest.completeTest);

async function runTest() {
  const response = await Protocol.Debugger.onceScriptParsed(2);
  const scriptIds = response.map(msg => msg.params.scriptId);

  await checkValidSkipLists(scriptIds[0], scriptIds[1]);
  await checkInvalidSkipLists(scriptIds[0]);
}

async function checkInvalidSkipLists(scriptId) {
  const actions = ['stepOver', 'stepInto'];
  for (let action of actions) {
    InspectorTest.log('Test: start position has invalid column number');
    let skipList = [createLocationRange(
        scriptId, first_non_debug_line_offset, -1, last_line_line_offset, 0)];
    await testStep(skipList, action);

    InspectorTest.log('Test: start position has invalid line number');
    skipList =
        [createLocationRange(scriptId, -1, 0, first_non_debug_line_offset, 0)];
    await testStep(skipList, action);

    InspectorTest.log('Test: end position smaller than start position');
    skipList = [createLocationRange(
        scriptId, if_case_line_offset, 0, first_non_debug_line_offset, 0)];
    await testStep(skipList, action);

    InspectorTest.log('Test: skip list is not maximally merged');
    skipList = [
      createLocationRange(
          scriptId, first_non_debug_line_offset, 0, if_case_line_offset, 0),
      createLocationRange(
          scriptId, if_case_line_offset, 0, last_line_line_offset, 0)
    ];
    await testStep(skipList, action);

    InspectorTest.log('Test: skip list is not sorted');
    skipList = [
      createLocationRange(
          scriptId, function_call_line_offset, 0, last_line_line_offset, 0),
      createLocationRange(
          scriptId, first_non_debug_line_offset, 0, if_case_line_offset, 0)
    ];
    await testStep(skipList, action);
  }
}

async function checkValidSkipLists(testScriptId, addScriptId) {
  InspectorTest.log('Test: Stepping over without skip list');
  await testStep([], 'stepOver');

  InspectorTest.log('Test: Stepping over with skip list');
  let skipList = [
    createLocationRange(
        testScriptId, first_non_debug_line_offset, 0, if_case_line_offset, 0),
    createLocationRange(
        testScriptId, function_call_line_offset, 0, last_line_line_offset, 0)
  ];
  await testStep(skipList, 'stepOver');

  InspectorTest.log('Test: Stepping over start location is inclusive');
  skipList = [createLocationRange(
      testScriptId, function_call_line_offset, function_call_column_offset,
      last_line_line_offset, 0)];
  await testStep(skipList, 'stepOver');

  InspectorTest.log('Test: Stepping over end location is exclusive');
  skipList = [createLocationRange(
      testScriptId, first_non_debug_line_offset, 0, function_call_line_offset,
      function_call_column_offset)];
  await testStep(skipList, 'stepOver');

  InspectorTest.log('Test: Stepping into without skip list');
  skipList = [];
  await testStep(skipList, 'stepInto');

  InspectorTest.log(
      'Test: Stepping into with skip list, while call itself is skipped');
  skipList = [
    createLocationRange(
        addScriptId, add_first_line_line_offset, 0, add_last_line_line_offset,
        0),
    createLocationRange(
        testScriptId, first_non_debug_line_offset, 0,
        function_call_line_offset + 1, 0),
  ];
  await testStep(skipList, 'stepInto');

  InspectorTest.log('Test: Stepping into start location is inclusive');
  skipList = [
    createLocationRange(
        addScriptId, add_last_line_line_offset, add_last_line_column_offset,
        add_last_line_line_offset + 1, 0),
  ];
  await testStep(skipList, 'stepInto');

  InspectorTest.log('Test: Stepping into end location is exclusive');
  skipList = [
    createLocationRange(
        addScriptId, add_last_line_line_offset - 1, 0,
        add_last_line_line_offset, add_last_line_column_offset),
  ];
  await testStep(skipList, 'stepInto');
}

async function testStep(skipList, stepAction) {
  InspectorTest.log(
      `Testing ${stepAction} with skipList: ${JSON.stringify(skipList)}`);
  Protocol.Runtime.evaluate({expression: 'test(5);'});
  while (true) {
    const pausedMsg = await Protocol.Debugger.oncePaused();
    const topCallFrame = pausedMsg.params.callFrames[0];
    printCallFrame(topCallFrame);
    if (topCallFrame.location.lineNumber == last_line_line_offset) break;

    const stepOverMsg = await Protocol.Debugger[stepAction]({skipList});
    if (stepOverMsg.error) {
      InspectorTest.log(stepOverMsg.error.message);
      Protocol.Debugger.resume();
      return;
    }
  }
  await Protocol.Debugger.resume();
}

function createLocationRange(
    scriptId, startLine, startColumn, endLine, endColumn) {
  return {
    scriptId: scriptId,
        start: {lineNumber: startLine, columnNumber: startColumn},
        end: {lineNumber: endLine, columnNumber: endColumn}
  }
}

function printCallFrame(frame) {
  InspectorTest.log(
      frame.functionName + ': ' + frame.location.lineNumber + ':' +
      frame.location.columnNumber);
}
