// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Flags: --noanalyze-environment-liveness

// Test that a variable in the local scope that shadows a context-allocated
// variable is correctly resolved when being evaluated in the debugger.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-131994.');

session.setupScriptMap();
contextGroup.addScript(`
function h() {
  var x;  // Context-allocated due to g().

  var g = function g() {
    x = -7;
  };

  var f = function f() {
    var x = 3;  // Allocated in the local scope.
    debugger;  // Break.
  };

  f();
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evalPromise = Protocol.Runtime.evaluate({expression: 'h()'});
  const {params: {callFrames}} = await pausedPromise;
  await session.logSourceLocation(callFrames[0].location);
  const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId: callFrames[0].callFrameId, expression: 'x'});
  InspectorTest.log('x -> ' + result.value);
  await Protocol.Debugger.resume();
  await evalPromise;
  InspectorTest.completeTest();
})();
