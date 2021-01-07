// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --no-enable-experimental-regexp-engine
// Flags: --no-enable-experimental-regexp-engine-on-excessive-backtracks
// Flags: --no-regexp-tier-up

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Checks pause inside blackboxed optimized function call.');
session.setupScriptMap();

(async function test(){
  Protocol.Debugger.enable();
  // Use an non-atomic regexp to trigger the actual regexp engine.
  Protocol.Debugger.setBlackboxPatterns({patterns: ['.*\.js']});
  // Run a script dominated by a regexp, with a sourceURL to pattern-match.
  Protocol.Runtime.evaluate({expression: `
    /((a*)*)*b/.exec('aaaaaaaaaaaaaa');
  //# sourceURL=framework.js`});
  // Issue lots of pause messages to trigger at least one during regexp.
  for (let i = 0; i < 100; i++) Protocol.Debugger.pause();
  // Send the messages through interupt while regexp is running.
  utils.interruptForMessages();
  InspectorTest.completeTest();
})();
