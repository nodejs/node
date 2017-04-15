// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Check that console.log doesn't run microtasks.");

InspectorTest.addScript(
`
function testFunction()
{
  Promise.resolve().then(function(){ console.log(239); });
  console.log(42);
  console.log(43);
}`);

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(messageAdded);
Protocol.Runtime.evaluate({ "expression": "testFunction()" });
Protocol.Runtime.evaluate({ "expression": "setTimeout(() => console.log(\"finished\"), 0)" });

function messageAdded(result)
{
  InspectorTest.logObject(result.params.args[0]);
  if (result.params.args[0].value === "finished")
    InspectorTest.completeTest();
}
