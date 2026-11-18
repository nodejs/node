// Copyright 2013 the V8 project authors. All rights reserved.
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

// If a function parameter is forced to be context allocated,
// debug evaluate need to resolve it to a context slot instead of
// parameter slot on the stack.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-325676.');

contextGroup.addScript(`
function f(arg) {
  debugger;
  console.log(arg);

  arg = "value";
  debugger;
  console.log(arg);

  // Forces arg to be context allocated even though a parameter.
  function g() { arg; }
}
`);

(async function test() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  Protocol.Runtime.onConsoleAPICalled(message => {
    InspectorTest.log(`after debugger: ${message.params.args[0].value}`);
  });

  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'arg',
    });
    InspectorTest.log(`at debugger: ${result.value}`);
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'arg = "evaluated";',
    });
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f(); f(1); f(1, 2);'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
