// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into constructors.');

const source = `
var done;

class Base {
  constructor() {
    var x = 1;    // 1.
    var y = 2;    // 2.
    done = true;  // 3.
  }
}

class Derived extends Base {}  // 0.

class Derived2 extends Derived {}  // -1.

var bound = Derived.bind(null);

function testStepInto() {
  new Derived();  // -1.
}

function testExtraIndirection() {
  new Derived2();  // -2.
}

function testBoundClass() {
  new bound();  // -1.
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let stepCount = 0;
  let allLinesMatched = true;

  Protocol.Debugger.onPaused(async message => {
    let callFrames = message.params.callFrames;
    let loc = callFrames[0].location;
    let line = source.split('\n')[loc.lineNumber];
    InspectorTest.log(`Pause at: ${line.trim()}`);
    const {result: {result: doneRes}} =
        await Protocol.Runtime.evaluate({expression: 'done'});
    if (!doneRes.value) {
      if (!line.includes('// ' + stepCount + '.')) {
        allLinesMatched = false;
        InspectorTest.log(
            `FAILED: expected // ${stepCount}. but got: ${line.trim()}`);
      }
      stepCount++;
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  function findLine(str) {
    return source.split('\n').findIndex(line => line.includes(str));
  }

  InspectorTest.log('--- TestBreakPointInConstructor ---');
  await Protocol.Runtime.evaluate({expression: 'done = false;'});
  stepCount = 1;
  let bp = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: findLine('var x = 1;    // 1.'),
  });
  await Protocol.Runtime.evaluate({expression: 'new Base();'});
  InspectorTest.log(`stepCount: ${stepCount}`);
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp.result.breakpointId});

  InspectorTest.log('--- TestDefaultConstructor ---');
  await Protocol.Runtime.evaluate({expression: 'done = false;'});
  stepCount = 1;
  bp = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: findLine('var x = 1;    // 1.'),
  });
  await Protocol.Runtime.evaluate({expression: 'new Derived();'});
  InspectorTest.log(`stepCount: ${stepCount}`);
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp.result.breakpointId});

  InspectorTest.log('--- TestStepInto ---');
  await Protocol.Runtime.evaluate({expression: 'done = false;'});
  stepCount = -1;
  bp = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: findLine('new Derived();  // -1.'),
  });
  await Protocol.Runtime.evaluate({expression: 'testStepInto();'});
  InspectorTest.log(`stepCount: ${stepCount}`);
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp.result.breakpointId});

  InspectorTest.log('--- TestExtraIndirection ---');
  await Protocol.Runtime.evaluate({expression: 'done = false;'});
  stepCount = -2;
  bp = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: findLine('new Derived2();  // -2.'),
  });
  await Protocol.Runtime.evaluate({expression: 'testExtraIndirection();'});
  InspectorTest.log(`stepCount: ${stepCount}`);
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp.result.breakpointId});

  InspectorTest.log('--- TestBoundClass ---');
  await Protocol.Runtime.evaluate({expression: 'done = false;'});
  stepCount = -1;
  bp = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: findLine('new bound();  // -1.'),
  });
  await Protocol.Runtime.evaluate({expression: 'testBoundClass();'});
  InspectorTest.log(`stepCount: ${stepCount}`);
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp.result.breakpointId});

  InspectorTest.log(`allLinesMatched: ${allLinesMatched}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
