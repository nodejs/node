// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Regression test for 485672657: UAF in RuntimeAgent');

// The script is executed when a context is created. The console messages
// are cached in V8ConsoleMessageStorage.
contextGroup.addScript(`
  var error = new Error();
  Object.defineProperty(error, 'name', {
      get: function() {
          console.clear();
          return "boom";
      }
  });
  // Log two messages. The getter on the first one will clear the storage.
  console.log(error);
  console.log("another message");
  //# sourceURL=test.js
`);
InspectorTest.log('Script added, messages will be cached upon context creation.');

InspectorTest.runAsyncTestSuite([
  async function test() {
    // Before the fix, enabling the runtime agent would iterate over the
    // cached messages. While processing the first message, the getter would
    // clear the storage, leading to a UAF when trying to access the second
    // message. This should not crash.
    await Protocol.Runtime.enable();
    InspectorTest.log('Runtime.enable finished without crash.');
  }
]);
