// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-491536.');

contextGroup.addScript(`
var source = "var outer = 0; function test() {'use strict'; outer = 1; } test(); console.log('ok');";
function test_function() { eval(source); }
`);

(async function test() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  Protocol.Runtime.onConsoleAPICalled(message => {
    InspectorTest.log(message.params.args[0].value);
  });

  const {result} = await Protocol.Runtime.evaluate({
    expression: 'test_function()',
  });
  InspectorTest.logMessage(result);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
