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

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-171715.');

contextGroup.addScript(`
var v = 'outer';
function run1() {  // Test 1 and 2
  var v = 'inner0';
  eval("debugger; var v = 'inner1'; debugger;");
  return v;
}

function run2() {  // Test 3
  var v = 'inner0';  // Local eval overwrites this value.
  eval("var v = 'inner1'; " +
       "(function() { var v = 'inner2'; debugger; })('x', 'y', 'z');");
  return v;
}
`);

async function evalV(callFrame) {
  const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: callFrame.callFrameId,
    expression: 'v',
  });
  return result.value;
}

async function evalArguments(callFrame) {
  const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: callFrame.callFrameId,
    expression: 'arguments',
  });
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
  return values;
}

(async function test() {
  await Protocol.Debugger.enable();

  let step = 0;
  Protocol.Debugger.onPaused(async message => {
    step++;
    const frames = message.params.callFrames;
    InspectorTest.log(`--- pause ${step} ---`);
    if (step === 1 || step === 2) {
      InspectorTest.log(`v[0]: ${await evalV(frames[0])}`);
      InspectorTest.log(`v[1]: ${await evalV(frames[1])}`);
      InspectorTest.log(`v[2]: ${await evalV(frames[2])}`);
      InspectorTest.log(
          `args[0]: ${JSON.stringify(await evalArguments(frames[0]))}`);
    } else {
      InspectorTest.log(`v[0]: ${await evalV(frames[0])}`);
      InspectorTest.log(`v[1]: ${await evalV(frames[1])}`);
      InspectorTest.log(`v[2]: ${await evalV(frames[2])}`);
      InspectorTest.log(`v[3]: ${await evalV(frames[3])}`);
      InspectorTest.log(
          `args[0]: ${JSON.stringify(await evalArguments(frames[0]))}`);
      InspectorTest.log(
          `args[1]: ${JSON.stringify(await evalArguments(frames[1]))}`);
    }
    Protocol.Debugger.resume();
  });

  const res1 = await Protocol.Runtime.evaluate({
    expression: 'run1("a", "b", "c")',
  });
  InspectorTest.log(`run1 result: ${res1.result.result.value}`);

  const res2 = await Protocol.Runtime.evaluate({
    expression: 'run2("a", "b", "c")',
  });
  InspectorTest.log(`run2 result: ${res2.result.result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
