// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'This test runs asm.js which calls back to JS. JS triggers a break, on ' +
    'pause we set breakpoints in the asm.js code.');

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

  %OptimizeFunctionOnNextCall(generateAsmJs);
  var fun = generateAsmJs(this, {'call_debugger': call_debugger}, undefined);
  fun();

  var finished = 'finished';
  debugger;
}

Protocol.Debugger.onPaused(handleDebuggerPaused);
Protocol.Debugger.onScriptParsed(handleScriptParsed);

function printResultAndContinue(next, message) {
  if (message.result && message.result.exceptionDetails)
    InspectorTest.logMessage(message.result.exceptionDetails);
  else if (message.error)
    InspectorTest.logMessage(message.error);
  else if (message.result && message.result.type !== undefined)
    InspectorTest.logMessage(message.result);
  if (next) next();
}

InspectorTest.runTestSuite([
  function enableDebugger(next) {
    Protocol.Debugger.enable().then(next);
  },

  function addScript(next) {
    afterScriptParsedCallback = next;
    contextGroup.addScript(testFunction.toString());
  },

  function runTestFunction(next) {
    afterFinishedTestFunctionCallback = next;
    Protocol.Runtime.evaluate({'expression': 'testFunction()'})
        .then(printResultAndContinue.bind(null, null));
  },

  function finished(next) {
    InspectorTest.log('Finished TestSuite.');
    next();
  },
]);

function locationToString(callFrame) {
  var res = {functionName: callFrame.functionName};
  for (var attr in callFrame.functionLocation) {
    if (attr == 'scriptId') continue;
    res['function_' + attr] = callFrame.functionLocation[attr];
  }
  for (var attr in callFrame.location) {
    if (attr == 'scriptId') continue;
    res[attr] = callFrame.location[attr];
  }
  return JSON.stringify(res);
}

function logStackTrace(messageObject) {
  var frames = messageObject.params.callFrames;
  for (var i = 0; i < frames.length; ++i) {
    InspectorTest.log('  - [' + i + '] ' + locationToString(frames[i]));
  }
}

var numPaused = 0;
var parsedScriptParams;

function handleDebuggerPaused(messageObject)
{
  ++numPaused;
  InspectorTest.log('Paused #' + numPaused);
  logStackTrace(messageObject);

  function cont() {
    var topFrameId = messageObject.params.callFrames[0].callFrameId;
    Protocol.Debugger
        .evaluateOnCallFrame({
          callFrameId: topFrameId,
          expression: 'typeof finished'
        })
        .then(callbackEvaluate);
    function callbackEvaluate(message) {
      var finished = message.result && message.result.result &&
          message.result.result.value === 'string';

      InspectorTest.log('Resuming...');
      Protocol.Debugger.resume();

      if (finished)
        afterFinishedTestFunctionCallback();
    }
  }

  if (numPaused > 1) {
    cont();
    return;
  }

  InspectorTest.log('First time paused, setting breakpoints!');

  var startLine = parsedScriptParams.startLine;
  var endLine = parsedScriptParams.endLine;
  InspectorTest.log(
      'Flooding script with breakpoints for all lines (' + startLine + ' - ' +
      endLine + ')...');
  var currentLine = startLine;
  function setNextBreakpoint(message) {
    if (message) InspectorTest.logMessage('error: ' + message.error);
    if (currentLine == endLine) {
      cont();
      return;
    }
    var thisLine = currentLine;
    currentLine += 1;
    InspectorTest.log('Setting breakpoint on line ' + thisLine);
    Protocol.Debugger
        .setBreakpoint({
          'location': {
            'scriptId': parsedScriptParams.scriptId,
            'lineNumber': thisLine
          }
        })
        .then(setNextBreakpoint);
  }
  setNextBreakpoint();
}

var numScripts = 0;

function handleScriptParsed(messageObject)
{
  var scriptId = messageObject.params.scriptId;
  ++numScripts;
  InspectorTest.log('Script nr ' + numScripts + ' parsed!');
  if (numScripts == 1) {
    parsedScriptParams = JSON.parse(JSON.stringify(messageObject.params));
    afterScriptParsedCallback();
  }
}
