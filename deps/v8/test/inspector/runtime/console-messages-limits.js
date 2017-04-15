// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print('Checks that console message storage doesn\'t exceed limits');

InspectorTest.addScript(`
function generateEmptyMessages(n) {
  for (var i = 0; i < n; ++i) {
    console.log('');
  }
}

function generate1MbMessages(n) {
  for (var i = 0; i < n; ++i) {
    console.log(new Array(1024 * 1024 - 32).join('!'));
  }
}
//# sourceURL=test.js`, 7, 26);

var messagesReported = 0;
Protocol.Runtime.onConsoleAPICalled(message => {
  ++messagesReported;
});

InspectorTest.runTestSuite([
  function testMaxConsoleMessagesCount(next) {
    messagesReported = 0;
    Protocol.Runtime.evaluate({ expression: 'generateEmptyMessages(1005)'})
      .then(() => Protocol.Runtime.enable())
      .then(() => Protocol.Runtime.disable())
      .then(() => InspectorTest.log(`Messages reported: ${messagesReported}`))
      .then(next);
  },

  function testMaxConsoleMessagesV8Size(next) {
    messagesReported = 0;
    Protocol.Runtime.evaluate({ expression: 'generate1MbMessages(11)'})
      .then(() => Protocol.Runtime.enable())
      .then(() => Protocol.Runtime.disable())
      .then(() => InspectorTest.log(`Messages reported: ${messagesReported}`))
      .then(next);
  }
]);
