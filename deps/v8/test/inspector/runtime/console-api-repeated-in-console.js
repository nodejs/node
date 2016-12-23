// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Check that console.log is reported through Console domain as well.");

var expectedMessages = 4;
var messages = [];

Protocol.Runtime.onConsoleAPICalled(consoleAPICalled);
Protocol.Console.onMessageAdded(messageAdded);
Protocol.Runtime.enable();
Protocol.Console.enable();
Protocol.Runtime.evaluate({ "expression": "console.log(42)" });
Protocol.Runtime.evaluate({ "expression": "console.error('abc')" });

function consoleAPICalled(result)
{
  messages.push("api call: " + result.params.args[0].value);
  if (!(--expectedMessages))
    done();
}

function messageAdded(result)
{
  messages.push("console message: " + result.params.message.text);
  if (!(--expectedMessages))
    done();
}

function done()
{
  messages.sort();
  for (var message of messages)
    InspectorTest.log(message);
  InspectorTest.completeTest();
}
