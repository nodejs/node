// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Break on exceptions from compiler errors.');

Protocol.Debugger.enable();
Protocol.Debugger.setPauseOnExceptions({state: 'all'});
Protocol.Debugger.onPaused(({params:{data}}) => {
  InspectorTest.log('paused on exception:');
  InspectorTest.logMessage(data);
  Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([
  async function testUnexpectedEndOfInput() {
    InspectorTest.log(`Runs '+++'`);
    let {result:{exceptionDetails}} = await Protocol.Runtime.evaluate({
      expression: '+++'
    });
    InspectorTest.log('Runtime.evaluate exceptionDetails:');
    InspectorTest.logMessage(exceptionDetails);
  },

  async function testUnexpectedIdentifier() {
    InspectorTest.log(`Runs 'x x'`);
    let {result:{exceptionDetails}} = await Protocol.Runtime.evaluate({
      expression: 'x x'
    });
    InspectorTest.log('Runtime.evaluate exceptionDetails:');
    InspectorTest.logMessage(exceptionDetails);
  },

  async function testEvalUnexpectedEndOfInput() {
    InspectorTest.log(`Runs eval('+++')`);
    let {result:{exceptionDetails}} = await Protocol.Runtime.evaluate({
      expression: `eval('+++')`
    });
    InspectorTest.log('Runtime.evaluate exceptionDetails:');
    InspectorTest.logMessage(exceptionDetails);
  },

  async function testEvalUnexpectedIdentifier() {
    InspectorTest.log(`Runs eval('x x')`);
    let {result:{exceptionDetails}} = await Protocol.Runtime.evaluate({
      expression: `eval('x x')`
    });
    InspectorTest.log('Runtime.evaluate exceptionDetails:');
    InspectorTest.logMessage(exceptionDetails);
  }
]);
