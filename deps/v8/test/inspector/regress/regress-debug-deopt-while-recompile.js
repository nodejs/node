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

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-debug-deopt-while-recompile.');

contextGroup.addScript(
    `
var f = function() {
  var bar = "foo";
  var baz = bar;  // Break point should be here.
  return bar;
};

var g = function() {
  var bar = "foo";
  var baz = bar;  // Break point should be here.
  return bar;
};

function warmup() {
  %PrepareFunctionForOptimization(f);
  %PrepareFunctionForOptimization(g);

  f();
  f();
  g();
  g();

  %OptimizeFunctionOnNextCall(f);
  if (%IsConcurrentRecompilationSupported()) {
    %OptimizeFunctionOnNextCall(g, "concurrent");
  }
}
`,
    0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Runtime.evaluate({expression: 'warmup()'});
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    const frame = message.params.callFrames[0];
    await session.logSourceLocation(frame.location);
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: frame.callFrameId, expression: 'bar'});
    InspectorTest.log('bar = ' + result.value);
    Protocol.Debugger.resume();
  });

  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 3, columnNumber: 0, url: 'test.js'});
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 9, columnNumber: 0, url: 'test.js'});

  await Protocol.Runtime.evaluate({expression: 'f(); g();'});
  InspectorTest.completeTest();
})();
