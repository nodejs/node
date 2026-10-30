// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-662674.');

contextGroup.addScript(`
function overflow() {
  return overflow();
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});

  Protocol.Debugger.onPaused(() => {
    Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate({
    expression: 'overflow()',
  });
  InspectorTest.log(
      `Exception: ${result.exceptionDetails.exception.className}`);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
