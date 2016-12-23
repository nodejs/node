// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Tests that Runtime.callFunctionOn works with awaitPromise flag.");

InspectorTest.runTestSuite([
  function testArguments(next)
  {
    callFunctionOn(
      "({a : 1})",
      "function(arg1, arg2, arg3, arg4) { return \"\" + arg1 + \"|\" + arg2 + \"|\" + arg3 + \"|\" + arg4; }",
      [ "undefined", "NaN", "({a:2})", "this"],
      /* returnByValue */ true,
      /* generatePreview */ false,
      /* awaitPromise */ false)
      .then((result) => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testSyntaxErrorInFunction(next)
  {
    callFunctionOn(
      "({a : 1})",
      "\n  }",
      [],
      /* returnByValue */ false,
      /* generatePreview */ false,
      /* awaitPromise */ true)
      .then((result) => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testExceptionInFunctionExpression(next)
  {
    callFunctionOn(
       "({a : 1})",
       "(function() { throw new Error() })()",
       [],
        /* returnByValue */ false,
        /* generatePreview */ false,
        /* awaitPromise */ true)
        .then((result) => InspectorTest.logMessage(result))
        .then(() => next());
  },

  function testFunctionReturnNotPromise(next)
  {
    callFunctionOn(
      "({a : 1})",
      "(function() { return 239; })",
      [],
      /* returnByValue */ false,
      /* generatePreview */ false,
      /* awaitPromise */ true)
      .then((result) => InspectorTest.logMessage(result.error))
      .then(() => next());
  },

  function testFunctionReturnResolvedPromiseReturnByValue(next)
  {
    callFunctionOn(
      "({a : 1})",
      "(function(arg) { return Promise.resolve({a : this.a + arg.a}); })",
      [ "({a:2})" ],
      /* returnByValue */ true,
      /* generatePreview */ false,
      /* awaitPromise */ true)
      .then((result) => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testFunctionReturnResolvedPromiseWithPreview(next)
  {
    callFunctionOn(
      "({a : 1})",
      "(function(arg) { return Promise.resolve({a : this.a + arg.a}); })",
      [ "({a:2})" ],
      /* returnByValue */ false,
      /* generatePreview */ true,
      /* awaitPromise */ true)
      .then((result) => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testFunctionReturnRejectedPromise(next)
  {
    callFunctionOn(
      "({a : 1})",
      "(function(arg) { return Promise.reject({a : this.a + arg.a}); })",
      [ "({a:2})" ],
      /* returnByValue */ true,
      /* generatePreview */ false,
      /* awaitPromise */ true)
      .then((result) => InspectorTest.logMessage(result))
      .then(() => next());
  }
]);

function callFunctionOn(objectExpression, functionDeclaration, argumentExpressions, returnByValue, generatePreview, awaitPromise)
{
  var objectId;
  var callArguments = [];
  var promise = Protocol.Runtime.evaluate({ expression: objectExpression })
    .then((result) => objectId = result.result.result.objectId)
  for (let argumentExpression of argumentExpressions) {
    promise = promise
      .then(() => Protocol.Runtime.evaluate({ expression: argumentExpression }))
      .then((result) => addArgument(result.result.result));
  }
  return promise.then(() => Protocol.Runtime.callFunctionOn({ objectId: objectId, functionDeclaration: functionDeclaration, arguments: callArguments, returnByValue: returnByValue, generatePreview: generatePreview, awaitPromise: awaitPromise }));

  function addArgument(result)
  {
    if (result.objectId) {
      callArguments.push({ objectId: result.objectId });
    } else if (result.value) {
      callArguments.push({ value: result.value })
    } else if (result.unserializableValue) {
      callArguments.push({ unserializableValue: result.unserializableValue });
    } else if (result.type === "undefined") {
      callArguments.push({});
    } else {
      InspectorTest.log("Unexpected argument object:");
      InspectorTest.logMessage(result);
      InspectorTest.completeTest();
    }
  }
}
