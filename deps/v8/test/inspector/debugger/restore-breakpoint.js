// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that debugger agent uses source content to restore breakpoints.');

Protocol.Debugger.enable();
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

var finishedTests = 0;
async function test(source, newSource, location, next) {
  var firstBreakpoint = true;
  Protocol.Debugger.onBreakpointResolved(message => {
    var lineNumber = message.params.location.lineNumber;
    var columnNumber = message.params.location.columnNumber;
    var currentSource = firstBreakpoint ? source : newSource;
    var lines = currentSource.split('\n');
    lines = lines.map(line => line.length > 80 ? line.substring(0, 77) + '...' : line);
    lines[lineNumber] = lines[lineNumber].slice(0, columnNumber) + '#' + lines[lineNumber].slice(columnNumber);
    InspectorTest.log(lines.join('\n'));
    firstBreakpoint = false;
  });

  var sourceURL = `test${++finishedTests}.js`;
  await Protocol.Debugger.setBreakpointByUrl({
    url: sourceURL,
    lineNumber: location.lineNumber,
    columnNumber: location.columnNumber
  });
  await Protocol.Runtime.evaluate({ expression: `${source}\n//# sourceURL=${sourceURL}` });
  await Protocol.Runtime.evaluate({ expression: `${newSource}\n//# sourceURL=${sourceURL}` });
  next();
}
