// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} =
    InspectorTest.start('Checks that Console.enable skips stored exceptions.');

Protocol.Console.onMessageAdded(({params}) => {
  InspectorTest.log(`Console: ${params.message.text}`);
});
Protocol.Runtime.onExceptionThrown(({params}) => {
  InspectorTest.log(`Runtime exception: ${params.exceptionDetails.text}`);
});

(async function test() {
  InspectorTest.log('Storing messages while both domains are disabled.');
  // Use a script so the exception is reported as uncaught.
  contextGroup.addScript(`
    console.log('before exception');
    throw new Error('stored exception');
  `);
  await Protocol.Runtime.evaluate(
      {expression: `console.log('after exception')`});

  InspectorTest.log('Enabling Console.');
  await Protocol.Console.enable();

  InspectorTest.log('Logging a live console message.');
  await Protocol.Runtime.evaluate({expression: `console.log('live message')`});

  // Console must leave the stored exception available to Runtime.
  InspectorTest.log('Enabling Runtime.');
  await Protocol.Runtime.enable();
  InspectorTest.completeTest();
})();
