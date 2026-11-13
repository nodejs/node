// Copyright 2011 the V8 project authors. All rights reserved.
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

// See: http://code.google.com/p/v8/issues/detail?id=1523

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-1523.');

session.setupScriptMap();
contextGroup.addScript(
    `
function test_and(x) {
  if (x && (bar === this.baz))
    return 0;
  return 1;
}

function test_or(x) {
  if (x || (bar === this.baz))
    return 0;
  return 1;
}`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 2, columnNumber: 0});
  await Protocol.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 8, columnNumber: 0});

  for (const expr of ['test_and(false)', 'test_or(true)']) {
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({expression: expr});
    const {params: {callFrames}} = await pausedPromise;
    await session.logSourceLocation(callFrames[0].location);
    await Protocol.Debugger.resume();
    const {result: {result}} = await evalPromise;
    InspectorTest.log(expr + ' -> ' + result.value);
  }

  InspectorTest.completeTest();
})();
