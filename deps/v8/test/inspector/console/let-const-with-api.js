// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests how let and const interact with command line api');

Protocol.Runtime.evaluate({ expression: "let a = 42;" }).then(step2);

function step2(response)
{
  failIfError(response);
  InspectorTest.log("first \"let a = 1;\" result: wasThrown = " + !!response.result.exceptionDetails);
  Protocol.Runtime.evaluate({ expression: "let a = 239;" }).then(step3);
}

function step3(response)
{
  failIfError(response);
  InspectorTest.log("second \"let a = 1;\" result: wasThrown = " + !!response.result.exceptionDetails);
  if (response.result.exceptionDetails)
    InspectorTest.log("exception message: " + response.result.exceptionDetails.text + " " + response.result.exceptionDetails.exception.description);
  Protocol.Runtime.evaluate({ expression: "a" }).then(step4);
}

function step4(response)
{
  failIfError(response);
  InspectorTest.log(JSON.stringify(response.result));
  checkMethod(null);
}

var methods = [ "dir", "dirxml", "keys", "values", "profile", "profileEnd",
    "inspect", "copy", "clear",
    "debug", "undebug", "monitor", "unmonitor", "table" ];

function checkMethod(response)
{
  failIfError(response);

  if (response)
    InspectorTest.log(response.result.result.description);

  var method = methods.shift();
  if (!method)
    InspectorTest.completeTest();

  Protocol.Runtime.evaluate({ expression: method, includeCommandLineAPI: true }).then(checkMethod);
}

function failIfError(response)
{
  if (response && response.error)
    InspectorTest.log("FAIL: " + JSON.stringify(response.error));
}
