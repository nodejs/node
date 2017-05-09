// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print('Checks that takeHeapSnapshot uses empty accessing_context for access \
checks.');

InspectorTest.addScript(`
function testFunction() {
  var array = [ createObjectWithStrictCheck() ];
  debugger;
}
//# sourceURL=test.js`);

Protocol.Debugger.onScriptParsed(message => {
  Protocol.HeapProfiler.takeHeapSnapshot({ reportProgress: false })
    .then(() => Protocol.Debugger.resume());
});

Protocol.Debugger.enable();
Protocol.HeapProfiler.enable();
Protocol.Runtime.evaluate({ expression: 'testFunction()' })
  .then(() => InspectorTest.log('Successfully finished'))
  .then(InspectorTest.completeTest);
