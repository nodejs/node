// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Check that setScriptSource does not crash when source is unchanged');

let scriptSource = `function TestExpression() {}`;
contextGroup.addScript(scriptSource);

(async function test() {
  Protocol.Debugger.enable();
  const {params: {scriptId}} = await Protocol.Debugger.onceScriptParsed();
  await Protocol.Debugger.setScriptSource({scriptId, scriptSource});
  InspectorTest.completeTest();
})();
