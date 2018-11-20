// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests scoped variable in Runtime.evaluate');

(async function test() {
  InspectorTest.log('Evaluating \'let a = 42;\'');
  var {result:{result}} = await Protocol.Runtime.evaluate({
    expression:'let a = 42;'});
  InspectorTest.logMessage(result);
  InspectorTest.log('Evaluating \'a\'');
  var {result:{result}} = await Protocol.Runtime.evaluate({
    expression:'a'});
  InspectorTest.logMessage(result);
  InspectorTest.log('Evaluating \'let a = 239;\'');
  var {result} = await Protocol.Runtime.evaluate({
    expression:'let a = 239;'});
  InspectorTest.logMessage(result);
  InspectorTest.log('Evaluating \'a\'');
  var {result:{result}} = await Protocol.Runtime.evaluate({
    expression:'a'});
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
