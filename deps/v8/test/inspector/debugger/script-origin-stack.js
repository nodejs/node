// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test(){
  const {session, contextGroup, Protocol} = InspectorTest.start(
      'Debugger.scriptParsed.stackTrace should contain only one frame');
  const { Debugger, Runtime } = Protocol;
  await Debugger.enable();
  Debugger.setAsyncCallStackDepth({ maxDepth: 32 });
  Runtime.evaluate({ expression: `setTimeout(() => eval(''), 0)` });
  await Debugger.onceScriptParsed();
  InspectorTest.logMessage(await Debugger.onceScriptParsed());
  InspectorTest.completeTest();
})()
