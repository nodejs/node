// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Regression test: step into an API function that has a user script.');

contextGroup.addScript(`
function donor() {}

// An API function: it can only break at entry and has no bytecode.
const victim = Sandbox.isWritable;

// Give the API function the script of a user function. It then looks subject
// to debugging, so stepping tries to flood it with one-shot breakpoints.
function corrupt() {
  const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const fieldOffset = (type, name) =>
      Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdFor(type), name);
  const kSfiOffset = fieldOffset('JS_FUNCTION_TYPE', 'shared_function_info');
  const kScriptOffset = fieldOffset('SHARED_FUNCTION_INFO_TYPE', 'script');
  const scriptField = f => {
    const sfi = memory.getUint32(Sandbox.getAddressOf(f) + kSfiOffset, true);
    return sfi - 1 + kScriptOffset;
  };
  memory.setUint32(
      scriptField(victim), memory.getUint32(scriptField(donor), true), true);
}

function trigger() {
  debugger;
  victim(donor);
}
`);

(async () => {
  await Protocol.Debugger.enable();
  const {result} = await Protocol.Runtime.evaluate({expression: 'corrupt()'});
  if (result.exceptionDetails) InspectorTest.logMessage(result);
  Protocol.Debugger.onPaused(() => Protocol.Debugger.stepInto());
  await Protocol.Runtime.evaluate({expression: 'trigger()'});
  // In sandbox testing mode a failed CHECK terminates the process without an
  // error, so make sure that execution got past the step.
  InspectorTest.log('Stepped past the API function.');
  InspectorTest.completeTest();
})();
