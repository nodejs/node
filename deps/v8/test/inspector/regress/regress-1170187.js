// Copyright 2008 the V8 project authors. All rights reserved.
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

// Make sure that the retrieval of local variables is performed correctly even
// when an adapter frame is present.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-1170187.');

contextGroup.addScript(`
function run() {
  return (function(x,y){
    var a,b,c;
    // Make sure a, b, and c are used.
    a,b,c;
    debugger;
    return 3
  })();
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evalPromise = Protocol.Runtime.evaluate({expression: 'run()'});
  const {params: {callFrames}} = await pausedPromise;
  const {result: {result: props}} = await Protocol.Runtime.getProperties({
    objectId: callFrames[0].scopeChain[0].object.objectId,
    ownProperties: true,
  });
  InspectorTest.logMessage(
      props.map(p => ({name: p.name, type: p.value.type})));
  await Protocol.Debugger.resume();
  const {result: {result}} = await evalPromise;
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
