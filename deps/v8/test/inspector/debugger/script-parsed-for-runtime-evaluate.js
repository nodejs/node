// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Checks that inspector reports script compiled in Runtime.evaluate, " +
  "Runtime.callFunctionOn and  Runtime.compileScript");

contextGroup.addScript(`
function fooTop() {
  eval(\`
    function foo() {
      eval("({})")
    }
    foo() //# sourceURL=second-frame.js\`);
}
//# sourceURL=top-frame.js`, 8, 26);

contextGroup.addScript(`
function fooTopFail() {
  eval(\`
    function fooFail() {
      eval("({}")
    }
    fooFail() //# sourceURL=second-frame-fail.js\`);
}
//# sourceURL=top-frame-fail.js`, 18, 26);

Promise.prototype.thenLog = function log(message) {
  return this.then(() => InspectorTest.log(message));
}

var objectId;
Protocol.Runtime.enable();
Protocol.Debugger.enable()
  .then(() => Protocol.Debugger.onScriptParsed(InspectorTest.logMessage))
  .then(() => Protocol.Debugger.onScriptFailedToParse(InspectorTest.logMessage))

  .thenLog('Runtime.evaluate with valid expression')
  .then(() => Protocol.Runtime.evaluate({
    expression: "({})//# sourceURL=evaluate.js"}))
  .then(msg => objectId = msg.result.result.objectId)

  .thenLog('Runtime.evaluate with syntax error')
  .then(() => Protocol.Runtime.evaluate({
    expression: "}//# sourceURL=evaluate-syntax-error.js"}))

  .thenLog('Runtime.callFunctionOn with valid functionDeclaration')
  .then(() => Protocol.Runtime.callFunctionOn({ objectId: objectId,
    functionDeclaration: "function foo(){}"}))

  .thenLog('Runtime.callFunctionOn with syntax error')
  .then(() => Protocol.Runtime.callFunctionOn({ objectId: objectId,
    functionDeclaration: "}"}))

  .thenLog('Runtime.compileScript with valid expression')
  .then(() => Protocol.Runtime.compileScript({ expression: "({})",
    sourceURL: "compile-script.js", persistScript: true }))

  .thenLog('Runtime.compileScript with syntax error')
  .then(() => Protocol.Runtime.compileScript({ expression: "}",
    sourceURL: "compile-script-syntax-error.js", persistScript: true }))

  .thenLog('Runtime.compileScript persistScript: false (should be no script events)')
  .then(() => Protocol.Runtime.compileScript({ expression: "({})",
    sourceURL: "compile-script-syntax-error.js", persistScript: false }))
  .then(() => Protocol.Runtime.compileScript({ expression: "}",
    sourceURL: "compile-script-syntax-error.js", persistScript: false }))

  .thenLog('Runtime.evaluate compiled script with stack trace')
  .then(() => Protocol.Runtime.evaluate({
    expression: "fooTop()"}))
  .then(msg => objectId = msg.result.result.objectId)

  .thenLog('Runtime.evaluate compile script error with stack trace')
  .then(() => Protocol.Runtime.evaluate({
    expression: "fooTopFail()"}))
  .then(msg => objectId = msg.result.result.objectId)

  .then(InspectorTest.completeTest);
