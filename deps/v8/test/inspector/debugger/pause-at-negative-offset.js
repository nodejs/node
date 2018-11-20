// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests that we dont crash on pause at negative offset');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  contextGroup.addScript(`debugger;//# sourceURL=test.js`, -3, -3);
  let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  session.logCallFrames(callFrames);
  InspectorTest.completeTest();
})();
