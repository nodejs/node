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

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-107996.');

contextGroup.addScript(`
var fourteen;

function f1() {
  var three = 3;
  var four = 4;
  (function f2() {
     var seven = 7;
     (function f3() {
        debugger;
        fourteen = three + four + seven;
     })();
  })();
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    const callFrames = message.params.callFrames;
    for (let i = 0; i < callFrames.length - 1; i++) {
      const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId: callFrames[i].callFrameId,
        expression: 'four',
        throwOnSideEffect: false,
      });
      InspectorTest.log(`frame ${i}: ${result.value}`);
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f1()'});
  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: 'fourteen',
  });
  InspectorTest.log(`fourteen: ${result.value}`);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
