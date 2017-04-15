// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Protocol.Runtime.enable();

addConsoleMessagePromise("console.log(239)")
  .then(message => InspectorTest.logMessage(message))
  .then(() => addConsoleMessagePromise("var l = console.log;\n  l(239)"))
  .then(message => InspectorTest.logMessage(message))
  .then(() => InspectorTest.completeTest());

function addConsoleMessagePromise(expression)
{
  var wait = Protocol.Runtime.onceConsoleAPICalled();
  Protocol.Runtime.evaluate({ expression: expression });
  return wait;
}
