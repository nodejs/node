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
//
// Flags: --noanalyze-environment-liveness

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-222893.');

contextGroup.addScript(`
var array = ["a", "b", "c"];

function f(a, b) {
  arguments;
  debugger;  // Arguments object is already materialized.
}

function g(a, b) {
  debugger;  // Arguments object is not yet materialized.
}

function h(a, b) {
  var arguments = undefined;
  debugger;  // Arguments already used as local variable.
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'arguments',
    });
    if (result.type === 'object' && result.objectId) {
      const {result: {result: props}} = await Protocol.Runtime.getProperties({
        objectId: result.objectId,
        ownProperties: true,
      });
      const values = [];
      for (const prop of props) {
        if (/^\d+$/.test(prop.name)) {
          values[Number(prop.name)] = prop.value.value;
        }
      }
      InspectorTest.log(`arguments: ${JSON.stringify(values)}`);
    } else {
      InspectorTest.log(`arguments: ${result.type}`);
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({
    expression: `
      f.apply(this, array);
      f("a", "b", "c");
      g.apply(this, array);
      g("a", "b", "c");
      h.apply(this, array);
      h("a", "b", "c");
    `,
  });
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
