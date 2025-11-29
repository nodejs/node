// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Check we\'re not pausing on breaks while installing console API');

(async function test(){
  // Set up additional console API to give evaluate() a chance to pause
  // there (which it shouldn't) while installing the API upon first eval.
  utils.setAdditionalConsoleApi(`function frobnicate() {
    return [...arguments].reverse().join(' ');
  }`);

  // Perform self-test, i.e. assure setAdditionalConsoleApi above has effect.
  Protocol.Runtime.enable();
  const expression = 'frobnicate("test", "self")';
  const {result} = await Protocol.Runtime.evaluate({
      expression,
      includeCommandLineAPI: true,
      returnByValue: true
  });
  InspectorTest.log(`${expression} = ${result.result.value}`);

  // Now for the actual test: get a clean context so that Runtime.evaluate
  // would install the API again.
  const contextGroup = new InspectorTest.ContextGroup();
  const session2 = contextGroup.connect();
  const Protocol2 = session2.Protocol;
  session2.setupScriptMap();

  Protocol2.Runtime.enable();
  Protocol2.Debugger.enable();
  await Protocol2.Debugger.pause();

  // Add a sourceURL to double check we're paused in the right place.
  const sourceURL = '//# sourceURL=the-right-place.js';
  Protocol2.Runtime.evaluate({
      expression: `frobnicate("real", "test");\n${sourceURL}`,
      includeCommandLineAPI: true
  });

  const paused = (await Protocol2.Debugger.oncePaused()).params;
  InspectorTest.log(
      `paused in: ${session2.getCallFrameUrl(paused.callFrames[0])}`);

  // Now if we're paused in the wrong place, we will likely crash.
  session2.disconnect();

  InspectorTest.quitImmediately();
})();
