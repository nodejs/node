// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests setting and clearing multiple breakpoints within functions.');

const source = `
function showBreakPoints(fn) {
  const src = %FunctionGetSourceCode(fn);
  const offset = %FunctionGetScriptSourcePosition(fn);
  const locations = %GetBreakLocations(fn);
  if (!locations) return src;
  locations.sort((x, y) => x - y);
  let result = '';
  let prev_pos = 0;
  for (let i = 0; i < locations.length; i++) {
    const pos = locations[i] - offset;
    result += src.slice(prev_pos, pos) + '[B' + i + ']';
    prev_pos = pos;
  }
  return result + src.substring(prev_pos, src.length);
}

function f() {a=1;b=2}
function g() {
  a=1;
  b=2;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  async function setBP(lineNumber, columnNumber) {
    const {result} = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber, columnNumber, url: 'test.js'});
    return result.breakpointId;
  }

  async function clearBP(breakpointId) {
    await Protocol.Debugger.removeBreakpoint({breakpointId});
  }

  async function logBreakPoints(fnName) {
    const {result: {result}} = await Protocol.Runtime.evaluate(
        {expression: `showBreakPoints(${fnName})`});
    InspectorTest.log(result.value);
  }

  InspectorTest.log('--- Function f (single line) ---');
  // function f() starts at line 17, column 9 (`() {a=1;b=2}`).
  let bp = await setBP(17, 9);
  await logBreakPoints('f');
  await clearBP(bp);
  await logBreakPoints('f');

  let bp1 = await setBP(17, 9 + 8);
  await logBreakPoints('f');
  let bp2 = await setBP(17, 9 + 4);
  await logBreakPoints('f');
  let bp3 = await setBP(17, 9 + 11);
  await logBreakPoints('f');
  await clearBP(bp1);
  await logBreakPoints('f');
  await clearBP(bp2);
  await logBreakPoints('f');
  await clearBP(bp3);
  await logBreakPoints('f');

  InspectorTest.log('--- Function g (multi-line) ---');
  // function g() starts at line 18, column 9.
  bp = await setBP(18, 9);
  await logBreakPoints('g');
  await clearBP(bp);
  await logBreakPoints('g');

  bp1 = await setBP(20, 0);
  await logBreakPoints('g');
  bp2 = await setBP(19, 0);
  await logBreakPoints('g');
  bp3 = await setBP(21, 0);
  await logBreakPoints('g');
  await clearBP(bp1);
  await logBreakPoints('g');
  await clearBP(bp2);
  await logBreakPoints('g');
  await clearBP(bp3);
  await logBreakPoints('g');

  InspectorTest.completeTest();
})();
