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

// Flags: --concurrent-recompilation --allow-natives-syntax

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-prepare-break-while-recompile.');

session.setupScriptMap();

const source = `
function foo() {
  return bar();
}

function bar() {
  var x = 2;
  return x;
}

function isUnoptimized(fn) {
  return (%GetOptimizationStatus(fn) & (1 << 3)) === 0;
}

function prepareAndKickOffOptimization() {
  %PrepareFunctionForOptimization(foo);
  %PrepareFunctionForOptimization(bar);
  foo();
  foo();
  %DisableOptimizationFinalization();
  %OptimizeFunctionOnNextCall(foo, "concurrent");
  foo();
  %WaitForBackgroundOptimization();
}

function finalizeOptimization() {
  %FinalizeOptimization();
}
`;

(async function test() {
  contextGroup.addScript(source, 0, 0, 'test.js');

  await Protocol.Runtime.evaluate(
      {expression: 'prepareAndKickOffOptimization()'});

  // Set break points on an inlined function. Clear the break point immediately
  // after to deactivate the debugger. Do all of this after the compile graph
  // has been created, but before the optimized code has been installed.
  await Protocol.Debugger.enable();
  const {result: {breakpointId, locations}} =
      await Protocol.Debugger.setBreakpointByUrl({
        lineNumber: 5,
        columnNumber: 0,
        url: 'test.js',
      });
  await session.logSourceLocation(locations[0]);
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  await Protocol.Debugger.disable();

  let {result: {result: {value: unopt1}}} =
      await Protocol.Runtime.evaluate({expression: 'isUnoptimized(foo)'});
  InspectorTest.log('foo isUnoptimized before finalize: ' + unopt1);

  // Install optimized code when concurrent optimization finishes.
  // This needs to be able to deal with shared code being a builtin.
  await Protocol.Runtime.evaluate({expression: 'finalizeOptimization()'});

  let {result: {result: {value: unopt2}}} =
      await Protocol.Runtime.evaluate({expression: 'isUnoptimized(foo)'});
  InspectorTest.log('foo isUnoptimized after finalize: ' + unopt2);

  InspectorTest.completeTest();
})();
