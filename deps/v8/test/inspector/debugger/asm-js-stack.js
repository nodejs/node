// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that asm-js scripts produce correct stack');

function testFunction() {
  function generateAsmJs(stdlib, foreign, heap) {
    'use asm';
    var debugger_fun = foreign.call_debugger;
    function callDebugger() {
      debugger_fun();
    }
    function redirectFun() {
      callDebugger();
    }
    return redirectFun;
  }

  function call_debugger() {
    debugger;
  }

  var fun = generateAsmJs(this, {'call_debugger': call_debugger}, undefined);
  fun();
}

contextGroup.addScript(testFunction.toString());

Protocol.Debugger.enable();
Protocol.Debugger.oncePaused().then(handleDebuggerPaused);
Protocol.Runtime.evaluate({'expression': 'testFunction()'});

function locationToString(callFrame) {
  var res = {functionName: callFrame.functionName};
  for (var attr in callFrame.functionLocation) {
    if (attr == 'scriptId') continue;
    res['function_'+attr] = callFrame.functionLocation[attr];
  }
  for (var attr in callFrame.location) {
    if (attr == 'scriptId') continue;
    res[attr] = callFrame.location[attr];
  }
  return JSON.stringify(res);
}

function logStackTrace(messageObject) {
  var frames = messageObject.params.callFrames;
  InspectorTest.log('Number of frames: ' + frames.length);
  for (var i = 0; i < frames.length; ++i) {
    InspectorTest.log('  - [' + i + '] ' + locationToString(frames[i]));
  }
}

function handleDebuggerPaused(messageObject)
{
  InspectorTest.log('Paused on \'debugger;\'');
  logStackTrace(messageObject);
  InspectorTest.log('Getting v8-generated stack trace...');
  var topFrameId = messageObject.params.callFrames[0].callFrameId;
  Protocol.Debugger
      .evaluateOnCallFrame({
        callFrameId: topFrameId,
        expression: '(new Error("getting stack trace")).stack'
      })
      .then(callbackEvaluate);
}

function callbackEvaluate(response)
{
  InspectorTest.log(
      'Result of evaluate (' + response.result.result.type + '):');
  var result_lines = response.result.result.value.split("\n");
  // Skip the second line, containing the 'evaluate' position.
  result_lines[1] = "    -- skipped --";
  InspectorTest.log(result_lines.join('\n'));
  InspectorTest.log('Finished!');
  InspectorTest.completeTest();
}
