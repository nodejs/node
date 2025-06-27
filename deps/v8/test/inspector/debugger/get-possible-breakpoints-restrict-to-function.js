// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Debugger.getPossibleBreakpoints with ignoreNestedFunctions');

var source = `
function test() {
  Array.from([1,2]).map(() => 1).filter(() => true);
  function nested1() {
    Array.from([1,2]).map(() => 1).filter(() => true);
  }
  function nested2() {
    Array.from([1,2]).map(() => 1).filter(() => true);
  }
  nested1();
  nested2();
}
//# sourceURL=test.js`;
contextGroup.addScript(source);

var scriptId;
Protocol.Debugger.onceScriptParsed().then(message => {
  if (message.params.url === 'test.js')
    scriptId = message.params.scriptId;
}).then(() => InspectorTest.runTestSuite(tests));

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  session.logSourceLocation(message.params.callFrames[0].location);
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
var tests = [
  function testWholeFunction(next) {
    Protocol.Debugger.getPossibleBreakpoints({ start: location(1, 18), ignoreNestedFunctions: false })
      .then(message => session.logBreakLocations(message.result.locations))
      .then(next);
  },

  function testWholeFunctionWithoutNested(next) {
    Protocol.Debugger.getPossibleBreakpoints({ start: location(1, 18), ignoreNestedFunctions: true })
      .then(message => session.logBreakLocations(message.result.locations))
      .then(next);
  },

  function testPartOfFunctionWithoutNested(next) {
    Protocol.Debugger.getPossibleBreakpoints({ start: location(1, 18), end: location(2, 18), ignoreNestedFunctions: true })
      .then(message => session.logBreakLocations(message.result.locations))
      .then(next);
  },

  function testNestedFunction(next) {
    Protocol.Debugger.getPossibleBreakpoints({ start: location(4, 0), ignoreNestedFunctions: true })
      .then(message => session.logBreakLocations(message.result.locations))
      .then(setAllBreakpoints)
      .then(() => InspectorTest.log('Run test() to check breakpoints..'))
      .then(() => Protocol.Runtime.evaluate({ expression: 'test()' }))
      .then(next);
  }
];

function location(lineNumber, columnNumber) {
  return { lineNumber: lineNumber, columnNumber: columnNumber, scriptId: scriptId };
}

function setAllBreakpoints(locations) {
  var promises = [];
  for (var location of locations)
    promises.push(Protocol.Debugger.setBreakpoint({ location: location }).then(checkBreakpoint));
  return Promise.all(promises);
}

function checkBreakpoint(message) {
  if (message.error) {
    InspectorTest.log('FAIL: error in setBreakpoint');
    InspectorTest.logMessage(message);
    return;
  }
  var id_data = message.result.breakpointId.split(':');
  if (parseInt(id_data[1]) !== message.result.actualLocation.lineNumber || parseInt(id_data[2]) !== message.result.actualLocation.columnNumber) {
    InspectorTest.log('FAIL: possible breakpoint was resolved in another location');
  }
}
