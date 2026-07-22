// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Runtime.evaluate returns object with undefined property.');

(async function test() {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: '({a:undefined,b:null,c:[1, null, undefined, 4]})',
    returnByValue: true
  }));
  InspectorTest.completeTest();
})();
