// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests conditional breakpoints at debugger statements and throw locations.');

contextGroup.addScript(
    `
function f(x) {
  debugger;
  return x + 1;
}

function g(x) {
  try {
    throw x;
  } catch (e) {
  }
}

function h(x) {
  var a = undefined;
  try {
    var x = a();
  } catch (e) {
  }
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  let exceptionCount = 0;
  Protocol.Debugger.onPaused(async ({params: {reason}}) => {
    if (reason === 'exception') {
      exceptionCount++;
    } else {
      breakCount++;
    }
    await Protocol.Debugger.resume();
  });

  function logCounts(label) {
    InspectorTest.log(
        label + ': breaks=' + breakCount + ', exceptions=' + exceptionCount);
  }

  logCounts('initial');
  await Protocol.Runtime.evaluate({expression: 'f(0)'});
  logCounts('after f(0)');
  await Protocol.Runtime.evaluate({expression: 'g(0)'});
  logCounts('after g(0)');

  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Runtime.evaluate({expression: 'f(0)'});
  logCounts('after setPauseOnExceptions f(0)');
  await Protocol.Runtime.evaluate({expression: 'g(0)'});
  logCounts('after setPauseOnExceptions g(0)');

  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 2,
    columnNumber: 0,
    url: 'test.js',
    condition: 'x == 1',
  });
  await Protocol.Runtime.evaluate({expression: 'f(1)'});
  logCounts('after bp(f, x == 1) f(1)');
  await Protocol.Runtime.evaluate({expression: 'f(2)'});
  logCounts('after bp(f, x == 1) f(2)');
  await Protocol.Runtime.evaluate({expression: 'f(1)'});
  logCounts('after bp(f, x == 1) f(1) again');

  let dupRes = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 2,
    columnNumber: 0,
    url: 'test.js',
    condition: 'x > 0',
  });
  InspectorTest.log('dup bp on f error: ' + Boolean(dupRes.error));
  await Protocol.Runtime.evaluate({expression: 'f(1)'});
  logCounts('after dup bp(f) f(1)');
  await Protocol.Runtime.evaluate({expression: 'f(0)'});
  logCounts('after dup bp(f) f(0)');

  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 8,
    columnNumber: 0,
    url: 'test.js',
    condition: '1 == 2',
  });
  await Protocol.Runtime.evaluate({expression: 'g(1)'});
  logCounts('after bp(g, 1 == 2) g(1)');

  dupRes = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 8,
    columnNumber: 0,
    url: 'test.js',
    condition: 'x == 1',
  });
  InspectorTest.log('dup bp on g (x == 1) error: ' + Boolean(dupRes.error));
  await Protocol.Runtime.evaluate({expression: 'g(1)'});
  logCounts('after dup bp(g, x == 1) g(1)');
  await Protocol.Runtime.evaluate({expression: 'g(2)'});
  logCounts('after dup bp(g, x == 1) g(2)');
  await Protocol.Runtime.evaluate({expression: 'g(1)'});
  logCounts('after dup bp(g, x == 1) g(1) again');

  dupRes = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 8,
    columnNumber: 0,
    url: 'test.js',
    condition: 'x > 0',
  });
  InspectorTest.log('dup bp on g (x > 0) error: ' + Boolean(dupRes.error));
  await Protocol.Runtime.evaluate({expression: 'g(1)'});
  logCounts('after dup bp(g, x > 0) g(1)');
  await Protocol.Runtime.evaluate({expression: 'g(0)'});
  logCounts('after dup bp(g, x > 0) g(0)');

  await Protocol.Runtime.evaluate({expression: 'h(0)'});
  logCounts('after h(0)');
  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 16,
    columnNumber: 0,
    url: 'test.js',
    condition: 'x > 0',
  });
  await Protocol.Runtime.evaluate({expression: 'h(1)'});
  logCounts('after bp(h, x > 0) h(1)');
  await Protocol.Runtime.evaluate({expression: 'h(0)'});
  logCounts('after bp(h, x > 0) h(0)');

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  InspectorTest.completeTest();
})();
