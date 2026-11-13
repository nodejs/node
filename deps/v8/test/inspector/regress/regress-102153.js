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

// Test that the break point is set before initializing the loop variable
// so that we break before any iteration has been run.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-102153.');

session.setupScriptMap();
contextGroup.addScript(
    `
function test() {
  for (var i = 0; i < 3; i++) {  // Break here.
    if (i == 0) break;
  }
}`,
    0, 0, 'test.js');

(async function runTest() {
  await Protocol.Debugger.enable();
  const {result: {locations}} = await Protocol.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 2, columnNumber: 0});
  await session.logSourceLocation(locations[0]);
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evalPromise = Protocol.Runtime.evaluate({expression: 'test()'});
  const {params: {callFrames}} = await pausedPromise;
  await session.logSourceLocation(callFrames[0].location);
  await Protocol.Debugger.resume();
  await evalPromise;
  InspectorTest.completeTest();
})();
