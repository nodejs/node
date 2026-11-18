// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that setting a breakpoint while active Sparkplug baseline code is ' +
    'on the stack preserves accumulator state.');

contextGroup.addScript(
    `
var foo = {
  [Symbol.toPrimitive]() { return "xyz"; }
};

function f() {
  var v = foo;
  var o = {[v]: v};
  return o["xyz"];
}
`,
    0, 0, 'test.js');

session.setupScriptMap();

(async function() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  Protocol.Runtime.onConsoleAPICalled(
      message => InspectorTest.log(message.params.args[0].value));
  let pauseCount = 0;
  Protocol.Debugger.onPaused(async message => {
    pauseCount++;
    InspectorTest.log(`pause #${pauseCount}:`);
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (pauseCount === 1) {
      InspectorTest.log('Setting breakpoint...');
      await Protocol.Debugger.setBreakpointByUrl({
        lineNumber: 8,
        columnNumber: 0,
        url: 'test.js',
      });
    }
    await Protocol.Debugger.resume();
  });
  await Protocol.Runtime.evaluate({
    expression: `
console.log(f() === foo);
%CompileBaseline(f);
console.log(f() === foo);

foo[Symbol.toPrimitive] = function(hint) {
  debugger;
  return "xyz";
};

console.log(f() === foo);
`,
  });
  InspectorTest.completeTest();
})();
