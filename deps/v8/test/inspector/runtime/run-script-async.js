// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that Runtime.compileScript and Runtime.runScript work with awaitPromise flag.");

InspectorTest.runTestSuite([
  function testRunAndCompileWithoutAgentEnable(next)
  {
    Protocol.Runtime.compileScript({ expression: "", sourceURL: "", persistScript: true })
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.runScript({ scriptId: "1" }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testSyntaxErrorInScript(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "\n }", sourceURL: "boo.js", persistScript: true }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testSyntaxErrorInEvalInScript(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "{\n eval(\"\\\n}\")\n}", sourceURL: "boo.js", persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testRunNotCompiledScript(next)
  {
    Protocol.Runtime.enable()
      .then((result) => Protocol.Runtime.runScript({ scriptId: "1" }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testRunCompiledScriptAfterAgentWasReenabled(next)
  {
    var scriptId;
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "{\n eval(\"\\\n}\")\n}", sourceURL: "boo.js", persistScript: true }))
      .then((result) => scriptId = result.result.scriptId)
      .then(() => Protocol.Runtime.disable())
      .then((result) => Protocol.Runtime.runScript({ scriptId: scriptId }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.enable())
      .then((result) => Protocol.Runtime.runScript({ scriptId: scriptId }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testRunScriptWithPreview(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "({a:1})", sourceURL: "boo.js", persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId, generatePreview: true }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testRunScriptReturnByValue(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "({a:1})", sourceURL: "boo.js", persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId, returnByValue: true }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testAwaitNotPromise(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "({a:1})", sourceURL: "boo.js", persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId, awaitPromise: true }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testAwaitResolvedPromise(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "Promise.resolve({a:1})", sourceURL: "boo.js", persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId, awaitPromise: true, returnByValue: true }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  },

  function testAwaitRejectedPromise(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: "Promise.reject({a:1})", sourceURL: "boo.js", persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId, awaitPromise: true, returnByValue: true }))
      .then((result) => InspectorTest.logMessage(result))
      .then(() => Protocol.Runtime.disable())
      .then(() => next());
  }
]);
