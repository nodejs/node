// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.addScript(
`function TestExpression(a, b) {
    return a + b;
}`);

// A general-purpose engine for sending a sequence of protocol commands.
// The clients provide requests and response handlers, while the engine catches
// errors and makes sure that once there's nothing to do completeTest() is called.
// @param step is an object with command, params and callback fields
function runRequestSeries(step) {
  processStep(step);

  function processStep(currentStep) {
    try {
      processStepOrFail(currentStep);
    } catch (e) {
      InspectorTest.log(e.stack);
      InspectorTest.completeTest();
    }
  }

  function processStepOrFail(currentStep) {
    if (!currentStep) {
      InspectorTest.completeTest();
      return;
    }
    if (!currentStep.command) {
      // A simple loopback step.
      var next = currentStep.callback();
      processStep(next);
      return;
    }

    var innerCallback = function(response) {
      var next;
      if ("error" in response) {
        if (!("errorHandler" in currentStep)) {
          // Error message is not logged intentionally, it may be platform-specific.
          InspectorTest.log("Protocol command '" + currentStep.command + "' failed");
          InspectorTest.completeTest();
          return;
        }
        try {
          next = currentStep.errorHandler(response.error);
        } catch (e) {
          InspectorTest.log(e.stack);
          InspectorTest.completeTest();
          return;
        }
      } else {
        try {
          next = currentStep.callback(response.result);
        } catch (e) {
          InspectorTest.log(e.stack);
          InspectorTest.completeTest();
          return;
        }
      }
      processStep(next);
    }
    var command = currentStep.command.split(".");
    Protocol[command[0]][command[1]](currentStep.params).then(innerCallback);
  }
}

function logEqualsCheck(actual, expected)
{
  if (actual === expected) {
    InspectorTest.log("PASS, result value: " + actual);
  } else {
    InspectorTest.log("FAIL, actual value: " + actual + ", expected: " + expected);
  }
}
function logCheck(description, success)
{
  InspectorTest.log(description + ": " + (success ? "PASS" : "FAIL"));
}

var firstStep = { callback: enableDebugger };

runRequestSeries(firstStep);

function enableDebugger() {
  return { command: "Debugger.enable", params: {}, callback: evalFunction };
}

function evalFunction(response) {
  var expression = "TestExpression(2, 4)";
  return { command: "Runtime.evaluate", params: { expression: expression }, callback: callbackEvalFunction };
}

function callbackEvalFunction(result) {
  InspectorTest.log("Function evaluate: " + JSON.stringify(result.result));
  logEqualsCheck(result.result.value, 6);

  return { command: "Runtime.evaluate", params: { expression: "TestExpression" }, callback: callbackEvalFunctionObject };
}

function callbackEvalFunctionObject(result) {
  return { command: "Runtime.getProperties", params: { objectId: result.result.objectId }, callback: callbackFunctionDetails };
}

function callbackFunctionDetails(result)
{
  var scriptId;
  for (var prop of result.internalProperties) {
    if (prop.name === "[[FunctionLocation]]")
      scriptId = prop.value.value.scriptId;
  }
  return createScriptManipulationArc(scriptId, null);
}

// Several steps with scriptId in context.
function createScriptManipulationArc(scriptId, next) {
  return { command: "Debugger.getScriptSource", params: { scriptId: scriptId }, callback: callbackGetScriptSource };

  var originalText;

  function callbackGetScriptSource(result) {
    originalText = result.scriptSource;
    var patched = originalText.replace("a + b", "a * b");

    return { command: "Debugger.setScriptSource", params: { scriptId: scriptId, scriptSource: patched }, callback: callbackSetScriptSource };
  }

  function callbackSetScriptSource(result) {
    var expression = "TestExpression(2, 4)";
    return { command: "Runtime.evaluate", params: { expression: expression }, callback: callbackEvalFunction2 };
  }

  function callbackEvalFunction2(result) {
    InspectorTest.log("Function evaluate: " + JSON.stringify(result.result));
    logEqualsCheck(result.result.value, 8);

    var patched = originalText.replace("a + b", "a # b");

    return { command: "Debugger.setScriptSource", params: { scriptId: scriptId, scriptSource: patched }, callback: errorCallbackSetScriptSource2 };
  }

  function errorCallbackSetScriptSource2(result) {
    var exceptionDetails = result.exceptionDetails;
    logCheck("Has error reported", !!exceptionDetails);
    logCheck("Reported error is a compile error", !!exceptionDetails);
    if (exceptionDetails)
      logEqualsCheck(exceptionDetails.lineNumber, 1);
    return next;
  }
}
