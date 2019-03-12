// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks console methods');

contextGroup.setupInjectedScriptEnvironment();
contextGroup.addScript(`
function testFunction() {
  console.debug('debug');
  console.error('error');
  console.info('info');
  console.log('log');
  console.warn('warn');
  console.dir('dir');
  console.dirxml('dirxml');
  console.table([[1,2],[3,4]]);
  console.table([[1,2],[3,4]], ['1','2']);
  console.trace('trace');
  console.trace();
  console.group();
  console.groupEnd();
  console.groupCollapsed();
  console.clear('clear');
  console.clear();
  console.count('count');
  function foo() {
    console.count();
  }
  foo();
  foo();
  console.count();
  console.count(undefined);
  console.count('default');
  console.countReset();
  console.count();
  console.countReset(undefined);
  console.count();
  console.countReset('default');
  console.count();
  console.countReset('countReset');
  console.table('foo');
  console.table(2);
  console.table(2n);
  console.table(Symbol('foo'));
  console.table(function() {});
}
//# sourceURL=test.js`, 7, 26);

Protocol.Runtime.onConsoleAPICalled(InspectorTest.logMessage);
Protocol.Runtime.enable();
Protocol.Runtime.evaluate({ expression: 'testFunction()' })
  .then(InspectorTest.completeTest);
