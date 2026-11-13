// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests setting multiple breakpoints at the same location.');

contextGroup.addScript(
    `
function f() { a = 1; b = 2; }
function g() { f(); }
function h() {}
`,
    0, 0, 'test.js');

class BreakpointError {
  #message = undefined;
  constructor(message) {
    this.#message = message;
  }
  get message() {
    return this.#message;
  }
}

async function setBreakPoint(lineNumber) {
  const res = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber,
    columnNumber: 0,
    url: 'test.js',
  });
  if (res.error) {
    throw new BreakpointError(res.error.message);
  }
  return res.result.breakpointId;
}

async function checkDuplicateThrows(lineNumber) {
  try {
    await setBreakPoint(lineNumber);
    return false;
  } catch (e) {
    return e instanceof BreakpointError;
  }
}

(async function test() {
  await Protocol.Debugger.enable();

  let breakPointHitCount = 0;
  Protocol.Debugger.onPaused(async () => {
    breakPointHitCount++;
    await Protocol.Debugger.resume();
  });

  // Set a breakpoint in f.
  breakPointHitCount = 0;
  let bp1 = await setBreakPoint(1);
  await Protocol.Runtime.evaluate({expression: 'f();'});
  InspectorTest.log('f hit count after bp1: ' + breakPointHitCount);

  // Set another breakpoint in f at the same place.
  InspectorTest.log(
      'duplicate bp in f has error: ' + await checkDuplicateThrows(1));
  await Protocol.Runtime.evaluate({expression: 'f();'});
  InspectorTest.log('f hit count after duplicate bp: ' + breakPointHitCount);

  // Remove the breakpoint.
  await Protocol.Debugger.removeBreakpoint({breakpointId: bp1});
  await Protocol.Runtime.evaluate({expression: 'f();'});
  InspectorTest.log(
      'f hit count after removeBreakpoint: ' + breakPointHitCount);

  // Perform the same test using function g.
  breakPointHitCount = 0;
  bp1 = await setBreakPoint(2);
  await Protocol.Runtime.evaluate({expression: 'g();'});
  InspectorTest.log('g hit count after bp1: ' + breakPointHitCount);
  InspectorTest.log(
      'duplicate bp in g has error: ' + await checkDuplicateThrows(2));
  await Protocol.Runtime.evaluate({expression: 'g();'});
  InspectorTest.log('g hit count after duplicate bp: ' + breakPointHitCount);
  await Protocol.Debugger.removeBreakpoint({breakpointId: bp1});
  await Protocol.Runtime.evaluate({expression: 'g();'});
  InspectorTest.log(
      'g hit count after removeBreakpoint: ' + breakPointHitCount);

  // Finally test with many breakpoints on h.
  const testCount = 10;
  breakPointHitCount = 0;
  for (let i = 0; i < testCount; i++) {
    if (i === 0) {
      await setBreakPoint(3);
    } else {
      if (!await checkDuplicateThrows(3)) {
        InspectorTest.log('Expected error on duplicate h bp');
      }
    }
    await Protocol.Runtime.evaluate({expression: 'h();'});
  }
  InspectorTest.log('h hit count after 10 calls: ' + breakPointHitCount);

  InspectorTest.completeTest();
})();
