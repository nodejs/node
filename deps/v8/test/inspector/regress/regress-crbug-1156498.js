// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests crbug.com/1156498.');

session.setupScriptMap();
contextGroup.addScript(`
function f(){
  (() => {
    var out = 42;
    var needsOut = () => {return out}
    var simp_class = class {
      static var1  = unreferenced;
    }
  })();
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    Protocol.Debugger.resume();
  });
  const {result: {exceptionDetails}} =
      await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(exceptionDetails.exception.description.split('\n')[0]);
  InspectorTest.completeTest();
})();
