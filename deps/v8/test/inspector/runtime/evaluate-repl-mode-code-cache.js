// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {Protocol} = InspectorTest.start(
  "Tests that Runtime.evaluate's REPL mode correctly interacts with the compliation cache (crbug.com/1108021)");

(async function() {
  InspectorTest.log('Prefill the cache with non-REPL mode script');
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: '5 + 3',
    replMode: false,
  }));

  InspectorTest.log('REPL mode scripts always return a Promise.')
  InspectorTest.log('The first script only returns "8" instead. When the inspector doesn\'t find a promise (due to a cache hit), it would respond with "undefined".');
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: '5 + 3',
    replMode: true,
  }));

  InspectorTest.completeTest();
})();
