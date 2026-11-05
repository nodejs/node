// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests conditional breakpoints.');

contextGroup.addScript(
    `
count = 0;
function f() {};
function g() {h(count++)};
function h(x) {var a=x; return a};
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let break_point_hit_count = 0;
  Protocol.Debugger.onPaused(async () => {
    break_point_hit_count++;
    await Protocol.Debugger.resume();
  });

  async function setBP(lineNumber, columnNumber, condition) {
    const response = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber, columnNumber, url: 'test.js', condition});
    if (response.error) {
      InspectorTest.log(`setBreakpoint error: ${response.error.message}`);
      return null;
    }
    return response.result.breakpointId;
  }

  async function clearBP(breakpointId) {
    await Protocol.Debugger.removeBreakpoint({breakpointId});
  }

  // Conditional breakpoint with syntax error.
  break_point_hit_count = 0;
  let bp = await setBP(2, 0, '{{{');
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`Syntax error condition hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Conditional breakpoint which evaluates to false.
  break_point_hit_count = 0;
  bp = await setBP(2, 0, 'false');
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`false condition hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Conditional breakpoint which evaluates to true.
  break_point_hit_count = 0;
  bp = await setBP(2, 0, 'true');
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`true condition hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Conditional breakpoint with different types of quotes.
  break_point_hit_count = 0;
  bp = await setBP(2, 0, '"a" == "a"');
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`double quotes condition hits: ${break_point_hit_count}`);
  await clearBP(bp);

  break_point_hit_count = 0;
  bp = await setBP(2, 0, '\'a\' == \'a\'');
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`single quotes condition hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Conditional breakpoint which checks global variable.
  break_point_hit_count = 0;
  bp = await setBP(2, 0, 'x==1');
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`x==1 before x defined hits: ${break_point_hit_count}`);
  await Protocol.Runtime.evaluate({expression: 'x=1; f()'});
  InspectorTest.log(`x==1 after x=1 hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Conditional breakpoint which checks changing global variable.
  break_point_hit_count = 0;
  bp = await setBP(3, 0, 'count % 2 == 0');
  await Protocol.Runtime.evaluate(
      {expression: 'for (var i = 0; i < 10; i++) g();'});
  InspectorTest.log(`count % 2 == 0 hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Conditional breakpoint which checks a parameter.
  break_point_hit_count = 0;
  bp = await setBP(4, 0, 'x % 2 == 0');
  await Protocol.Runtime.evaluate(
      {expression: 'for (var i = 0; i < 10; i++) g();'});
  InspectorTest.log(`x % 2 == 0 hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Conditional breakpoint which checks a local variable.
  break_point_hit_count = 0;
  bp = await setBP(4, 22, 'a % 2 == 0');
  await Protocol.Runtime.evaluate(
      {expression: 'for (var i = 0; i < 10; i++) g();'});
  InspectorTest.log(`a % 2 == 0 hits: ${break_point_hit_count}`);
  await clearBP(bp);

  // Multiple conditional breakpoints with the same condition.
  break_point_hit_count = 0;
  let bp1 = await setBP(4, 22, 'a % 2 == 0');
  await setBP(4, 22, 'a % 2 == 0');
  await Protocol.Runtime.evaluate(
      {expression: 'for (var i = 0; i < 10; i++) g();'});
  InspectorTest.log(`same condition duplicate hits: ${break_point_hit_count}`);
  await clearBP(bp1);

  // Multiple conditional breakpoints with different conditions.
  break_point_hit_count = 0;
  bp1 = await setBP(4, 22, 'a % 2 == 0');
  await setBP(4, 22, '(a + 1) % 2 == 0');
  await Protocol.Runtime.evaluate(
      {expression: 'for (var i = 0; i < 10; i++) g();'});
  InspectorTest.log(
      `different condition duplicate hits: ${break_point_hit_count}`);
  await clearBP(bp1);

  InspectorTest.completeTest();
})();
