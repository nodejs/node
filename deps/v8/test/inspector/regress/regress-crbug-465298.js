// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noturbo-inlining

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-465298.');

contextGroup.addScript(`
var stdlib = this;
var buffer = new ArrayBuffer(64 * 1024);
var foreign = { thrower: thrower, debugme: debugme };

function thrower() { throw "boom"; }
function debugme() { debugger; }

function Module(stdlib, foreign, heap) {
  "use asm";
  var thrower = foreign.thrower;
  var debugme = foreign.debugme;
  function foo(i) {
    i = i|0;
    var a = 101;
    var b = 102;
    var c = 103;
    if (i > 0) {
      debugme();
      i = 23;
    } else {
      thrower();
      i = 42;
    }
    return i|0;
  }
  return { foo: foo };
}

var m = Module(stdlib, foreign, buffer);
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(message => {
    const frames = message.params.callFrames;
    InspectorTest.log('frame 0: ' + frames[0].functionName);
    InspectorTest.log('frame 1: ' + frames[1].functionName);
    Protocol.Debugger.resume();
  });

  const res0 = await Protocol.Runtime.evaluate({expression: 'm.foo(0)'});
  InspectorTest.logMessage(res0);
  const res1 = await Protocol.Runtime.evaluate({expression: 'm.foo(1)'});
  InspectorTest.logMessage(res1);
  InspectorTest.completeTest();
})();
