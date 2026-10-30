// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api --sandbox-testing

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/502337304');

contextGroup.addScript(`
const NUM_PARAMS = 60000;
let paramTail = '';
for (let i = 0; i < NUM_PARAMS; i++) paramTail += ',p' + i;
const padLen = paramTail.length;
const head = 'function victim(a';
const tail = '){debugger;}';
const origSource = head + '/*' + 'X'.repeat(padLen - 4) + '*/' + tail;
const corruptSource = head + paramTail + tail;

// Indirect eval to get a Script object
(0, eval)(origSource);

function corrupt() {
  if (typeof Sandbox === 'undefined') return;

  let victim_addr = Sandbox.getAddressOf(victim);
  let view = new DataView(new Sandbox.MemoryView(victim_addr, 32));
  let sfi_ptr = view.getUint32(16, true);
  let sfi_addr = sfi_ptr - 1;

  let sfi_view = new DataView(new Sandbox.MemoryView(sfi_addr, 32));
  let script_ptr = sfi_view.getUint32(20, true);
  let script_addr = script_ptr - 1;

  let script_view = new DataView(new Sandbox.MemoryView(script_addr, 32));
  let corrupt_source_addr = Sandbox.getAddressOf(corruptSource);
  script_view.setUint32(4, corrupt_source_addr | 1, true);
}
`, 0, 0, 'setup.js');

Protocol.Debugger.onPaused(async message => {
  const callFrameId = message.params.callFrames[0].callFrameId;
  await Protocol.Runtime.evaluate({expression: 'corrupt()'});
  await Protocol.Debugger.setVariableValue({
    scopeNumber: 0,
    variableName: 'p59999',
    newValue: {value: 42},
    callFrameId: callFrameId,
  });
  await Protocol.Debugger.resume();
  InspectorTest.completeTest();
});

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'victim(1);'});
})();
