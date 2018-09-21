// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that debugger agent uses source content to restore breakpoints.');

Protocol.Debugger.enable();
var finishedTests = 0;
InspectorTest.runTestSuite([
  function testSameSource(next) {
    var source = 'function foo() {\nboo();\n}';
    test(source, source, { lineNumber: 1, columnNumber: 0 }, next);
  },

  function testOneLineOffset(next) {
    var source = 'function foo() {\nboo();\n}';
    var newSource = 'function foo() {\nboo();\nboo();\n}';
    test(source, newSource, { lineNumber: 1, columnNumber: 0 }, next);
  },

  function testTwoSimilarLinesCloseToOriginalLocation1(next) {
    var source = 'function foo() {\n\n\nboo();\n}';
    var newSource = 'function foo() {\nboo();\n\nnewCode();\nboo();\n\n\n\nboo();\n}';
    test(source, newSource, { lineNumber: 3, columnNumber: 0 }, next);
  },

  function testTwoSimilarLinesCloseToOriginalLocation2(next) {
    var source = 'function foo() {\n\n\nboo();\n}';
    var newSource = 'function foo() {\nboo();\nnewLongCode();\nnewCode();\nboo();\n\n\n\nboo();\n}';
    test(source, newSource, { lineNumber: 3, columnNumber: 0 }, next);
  },

  function testHintIgnoreWhiteSpaces(next) {
    var source = 'function foo() {\n\n\n\nboo();\n}';
    var newSource = 'function foo() {\nfoo();\n\n\nboo();\n}';
    test(source, newSource, { lineNumber: 1, columnNumber: 0 }, next);
  },

  function testCheckOnlyLimitedOffsets(next) {
    var source = 'function foo() {\nboo();\n}';
    var longString = ';'.repeat(1000);
    var newSource = `function foo() {\nnewCode();\n${longString};\nboo();\n}`;
    test(source, newSource, { lineNumber: 1, columnNumber: 0 }, next);
  }
]);

async function test(source, newSource, location, next) {
  function dumpSourceWithBreakpoint(source, location) {
    var lineNumber = location.lineNumber;
    var columnNumber = location.columnNumber;
    var lines = source.split('\n');
    lines = lines.map(line => line.length > 80 ? line.substring(0, 77) + '...' : line);
    lines[lineNumber] = lines[lineNumber].slice(0, columnNumber) + '#' + lines[lineNumber].slice(columnNumber);
    InspectorTest.log(lines.join('\n'));
  }

  Protocol.Debugger.onBreakpointResolved(message => {
    dumpSourceWithBreakpoint(newSource, message.params.location);
  })

  var sourceURL = `test${++finishedTests}.js`;
  await Protocol.Runtime.evaluate({ expression: `${source}\n//# sourceURL=${sourceURL}` });
  let {result:{locations}} = await Protocol.Debugger.setBreakpointByUrl({
    url: sourceURL,
    lineNumber: location.lineNumber,
    columnNumber: location.columnNumber
  });
  dumpSourceWithBreakpoint(source, locations[0]);
  await Protocol.Runtime.evaluate({ expression: `${newSource}\n//# sourceURL=${sourceURL}` });
  next();
}
