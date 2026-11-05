// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests setting and removing breakpoints on class fields and computed ' +
    'property names.');

const source = `
function showBreakPoints(f) {
  const src = %FunctionGetSourceCode(f);
  const offset = %FunctionGetScriptSourcePosition(f);
  const locations = %GetBreakLocations(f);
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

class Y {
  x = 1;
  y = 2;
  z = 3;
}

var initializer = %GetInitializerFunction(Y);

function foo() {}
var bar = 'bar';

class X {
  [foo()] = 1;
  baz = foo();
}

var initializerX = %GetInitializerFunction(X);

function t() {
  class X {
    [foo()] = 1;
    [bar] = 2;
    baz = foo();
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  async function setBP(lineNumber, columnNumber = 0) {
    const {result} = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber, columnNumber, url: 'test.js'});
    await session.logSourceLocation(result.locations[0]);
    return result.breakpointId;
  }

  async function removeBP(breakpointId) {
    await Protocol.Debugger.removeBreakpoint({breakpointId});
  }

  async function logBreakPoints(expr) {
    const {result: {result}} = await Protocol.Runtime.evaluate(
        {expression: `showBreakPoints(${expr})`});
    InspectorTest.log(result.value);
  }

  InspectorTest.log('--- class Y single breakpoints ---');
  let b1 = await setBP(18);
  await logBreakPoints('initializer');
  await removeBP(b1);
  await logBreakPoints('initializer');

  let b2 = await setBP(19);
  await logBreakPoints('initializer');
  await removeBP(b2);
  await logBreakPoints('initializer');

  let b3 = await setBP(20);
  await logBreakPoints('initializer');
  await removeBP(b3);
  await logBreakPoints('initializer');

  InspectorTest.log('--- class Y breakpoint pairs ---');
  b1 = await setBP(18);
  b2 = await setBP(19);
  await logBreakPoints('initializer');
  await removeBP(b1);
  await logBreakPoints('initializer');
  await removeBP(b2);
  await logBreakPoints('initializer');

  b1 = await setBP(18);
  b3 = await setBP(20);
  await logBreakPoints('initializer');
  await removeBP(b1);
  await logBreakPoints('initializer');
  await removeBP(b3);
  await logBreakPoints('initializer');

  b2 = await setBP(19);
  b3 = await setBP(20);
  await logBreakPoints('initializer');
  await removeBP(b2);
  await logBreakPoints('initializer');
  await removeBP(b3);
  await logBreakPoints('initializer');

  InspectorTest.log('--- top-level class X with computed property ---');
  b1 = await setBP(29, 2);
  await logBreakPoints('initializerX');
  await removeBP(b1);

  b1 = await setBP(30);
  await logBreakPoints('initializerX');
  await removeBP(b1);

  InspectorTest.log('--- function t() enclosing class X ---');
  b1 = await setBP(37);
  await logBreakPoints('t');
  await removeBP(b1);
  await logBreakPoints('t');

  b2 = await setBP(38);
  await logBreakPoints('t');
  await removeBP(b2);
  await logBreakPoints('t');

  b3 = await setBP(39);
  await logBreakPoints('t');
  await removeBP(b3);

  b1 = await setBP(37);
  b2 = await setBP(38);
  await logBreakPoints('t');
  await removeBP(b1);
  await logBreakPoints('t');
  await removeBP(b2);
  await logBreakPoints('t');

  b1 = await setBP(37);
  b3 = await setBP(39);
  await logBreakPoints('t');
  await removeBP(b1);
  await logBreakPoints('t');
  await removeBP(b3);

  b2 = await setBP(38);
  b3 = await setBP(39);
  await logBreakPoints('t');
  await removeBP(b2);
  await logBreakPoints('t');
  await removeBP(b3);

  InspectorTest.completeTest();
})();
