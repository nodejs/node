// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/513360060');

// Build a victim function whose original source is mostly a block comment,
// and a fake source string of identical length where the comment is replaced
// with many calls so reparsing produces a much larger SourcePositionTable.
const callCount = 100000;
const fakeBody = 'g();'.repeat(callCount) + 'return 0;';
const commentPadding = ' '.repeat(fakeBody.length - 13);
contextGroup.addScript(
    `function victim(g) {\n/*${commentPadding}*/return 0;\n}`,
    0, 0, 'victim.js');

contextGroup.addScript(`
const fakeSource = 'function victim(g) {\\n${fakeBody}\\n}';

function corruptScriptSource() {
  // Compile victim lazily without source positions.
  victim(() => 0);

  // Swap Script::source in the sandbox to fakeSource before the debugger
  // collects source positions via Compiler::CollectSourcePositions.
  const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const sfiAddr = Sandbox.readObjectField(victim, 'shared_function_info') - 1;
  const sfiType = Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE');
  const scriptAddr =
      memory.getUint32(sfiAddr + Sandbox.getFieldOffset(sfiType, 'script'), true) - 1;
  const scriptType = Sandbox.getInstanceTypeIdFor('SCRIPT_TYPE');
  const fakeSourceTagged = (Sandbox.getAddressOf(fakeSource) | 1) >>> 0;
  memory.setUint32(
      scriptAddr + Sandbox.getFieldOffset(scriptType, 'source'),
      fakeSourceTagged, true);
}
`);

(async () => {
  await Protocol.Runtime.evaluate({expression: 'corruptScriptSource()'});
  await Protocol.Debugger.enable();
  const bp = await Protocol.Debugger.setBreakpointByUrl({
    url: 'victim.js',
    lineNumber: 1,
    columnNumber: 0,
  });
  await Protocol.Debugger.removeBreakpoint({
    breakpointId: bp.result.breakpointId,
  });
  InspectorTest.completeTest();
})();
