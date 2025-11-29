// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests url in Debugger.CallFrame.');

contextGroup.addScript(`
eval('function foo1() { debugger; }');
eval('function foo2() { foo1() } //# sourceURL=source-url.js');
function foo3() { foo2(); }
`, 0, 0, 'test.js');

(async function test() {
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'foo3()//# sourceURL=expr.js'});
  let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.logMessage(callFrames.map(frame => ({url: frame.url})));
  InspectorTest.completeTest();
})();
