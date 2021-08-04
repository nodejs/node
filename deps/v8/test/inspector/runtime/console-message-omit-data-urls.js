// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we only send along non-data urls.');

var expectedMessages = 2;
var messages = [];

Protocol.Runtime.enable();
Protocol.Console.enable();

Protocol.Runtime.onConsoleAPICalled(consoleAPICalled);
Protocol.Runtime.onExceptionThrown(exceptionThrown);

contextGroup.addScript(`
async function test() {
  console.log("Hello World!");
  throw new Exception("Exception thrown");
}
//# sourceURL=data:,pseudoDataUrl`);

function consoleAPICalled(result)
{
  const msgText = result.params.args[0].value;
  const callFrames = result.params.stackTrace.callFrames;
  let messageParts = [];
  messageParts.push(`console api called: ${msgText}`);
  for (frame of callFrames) {
    messageParts.push(`  callFrame: function ${frame.functionName} (url: ${frame.url})`);
  }
  messages.push(messageParts.join("\n"));

  if (!(--expectedMessages)) {
    done();
  }
}

function exceptionThrown(result)
{
  const exceptionDetails = result.params.exceptionDetails;
  const url = exceptionDetails.url;
  const text = exceptionDetails.text;
  messages.push(`exception details: ${text} (url: ${url ? url : ""})`)

  if (!(--expectedMessages)) {
    done();
  }
}

function done()
{
  messages.sort();
  for (var message of messages) {
    InspectorTest.log(message);
  }
  InspectorTest.completeTest();
}

(async function test() {
  InspectorTest.log('Test log with data uri.');
  await Protocol.Runtime.evaluate({ expression: `test()//# sourceURL=test.js`});
})();
