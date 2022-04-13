// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Check different set breakpoint cases.');

contextGroup.addScript(
`function f() {
  a=1;
};

function g() {
  // Comment.
  f();
}

eval('function h(){}');
eval('function sourceUrlFunc() { a = 2; }\\n//# sourceURL=sourceUrlScript');`);

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  {
    InspectorTest.log('Set breakpoint at function g..');
    const location = await functionLocation('g');
    const {result} = await Protocol.Debugger.setBreakpoint({location});
    InspectorTest.log('Breakpoint set at:');
    await session.logSourceLocation(result.actualLocation);
    InspectorTest.log('Call g..');
    Protocol.Runtime.evaluate({expression: 'g()'});
    const {params:{
      callFrames:[topFrame],
      hitBreakpoints
    }} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Breakpoint hit at:');
    await session.logSourceLocation(topFrame.location);
    const hitBreakpoint = hitBreakpoints[0] === result.breakpointId;
    InspectorTest.log(`hitBreakpoints contains breakpoint: ${hitBreakpoint}\n`);
  }

  {
    InspectorTest.log('Set breakpoint at function f when we on pause..');
    const {result} = await Protocol.Debugger.setBreakpoint({
      location: await functionLocation('f')
    });
    InspectorTest.log('Breakpoint set at:');
    await session.logSourceLocation(result.actualLocation);
    InspectorTest.log('Resume..');
    Protocol.Debugger.resume();
    const {params:{
      callFrames:[topFrame],
      hitBreakpoints
    }} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Breakpoint hit at:');
    await session.logSourceLocation(topFrame.location);
    const hitBreakpoint = hitBreakpoints[0] === result.breakpointId;
    InspectorTest.log(`hitBreakpoints contains breakpoint: ${hitBreakpoint}\n`);
    await Protocol.Debugger.resume();
  }
  {
    InspectorTest.log('Set breakpoint by url at sourceUrlFunc..');
    const {lineNumber, columnNumber} = await functionLocation('sourceUrlFunc');
    const {result} = await Protocol.Debugger.setBreakpointByUrl({
      url: 'sourceUrlScript',
      lineNumber,
      columnNumber
    });
    InspectorTest.log('Breakpoint set at:');
    await session.logSourceLocation(result.locations[0]);
    InspectorTest.log('Call sourceUrlFunc..');
    Protocol.Runtime.evaluate({expression: 'sourceUrlFunc()'});
    const {params:{
      callFrames:[topFrame],
      hitBreakpoints
    }} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Breakpoint hit at:');
    await session.logSourceLocation(topFrame.location);
    const hitBreakpoint = hitBreakpoints[0] === result.breakpointId;
    InspectorTest.log(`hitBreakpoints contains breakpoint: ${hitBreakpoint}\n`);
    await Protocol.Debugger.resume();
  }

  {
    InspectorTest.log(
        'Set breakpoint at empty line by url in top level function..');
    const {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      url: 'test-script',
      lineNumber: 4,
      columnNumber: 0
    });
    Protocol.Runtime.evaluate({
      expression: `//# sourceURL=test-script\nfunction i1(){};\n\n\n\n\nfunction i2(){}\n// last line`
    });
    const [{params: {location}}] =
        await Promise.all([Protocol.Debugger.onceBreakpointResolved()]);
    InspectorTest.log('Breakpoint resolved at:');
    await session.logSourceLocation(location);
  }
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

async function functionLocation(name) {
  const {result:{result}} = await Protocol.Runtime.evaluate({expression: name});
  const {result:{internalProperties}} = await Protocol.Runtime.getProperties({
    objectId: result.objectId
  });
  const {value:{value}} = internalProperties.find(
      prop => prop.name === '[[FunctionLocation]]');
  return value;
}
