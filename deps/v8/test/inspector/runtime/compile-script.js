// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var executionContextId;

Protocol.Debugger.enable().then(onDebuggerEnabled);

function onDebuggerEnabled()
{
  Protocol.Runtime.enable();
  Protocol.Debugger.onScriptParsed(onScriptParsed);
  Protocol.Runtime.onExecutionContextCreated(onExecutionContextCreated);
}

function onScriptParsed(messageObject)
{
  if (!messageObject.params.url)
    return;
  InspectorTest.log("Debugger.scriptParsed: " + messageObject.params.url);
}

function onExecutionContextCreated(messageObject)
{
  executionContextId = messageObject.params.context.id;
  testCompileScript("\n  (", false, "foo1.js")
    .then(() => testCompileScript("239", true, "foo2.js"))
    .then(() => testCompileScript("239", false, "foo3.js"))
    .then(() => testCompileScript("testfunction f()\n{\n    return 0;\n}\n", false, "foo4.js"))
    .then(() => InspectorTest.completeTest());
}

function testCompileScript(expression, persistScript, sourceURL)
{
  InspectorTest.log("Compiling script: " + sourceURL);
  InspectorTest.log("         persist: " + persistScript);
  return Protocol.Runtime.compileScript({
    expression: expression,
    sourceURL: sourceURL,
    persistScript: persistScript,
    executionContextId: executionContextId
  }).then(onCompiled);

  function onCompiled(messageObject)
  {
    InspectorTest.log("compilation result: ");
    InspectorTest.logMessage(messageObject);
    InspectorTest.log("-----");
  }
}
