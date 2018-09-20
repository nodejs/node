// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that double numbers are parsed and serialized correctly on different locales');

(async function() {
  InspectorTest.log('This test verifies that we correctly parse doubles with non-US locale');
  utils.setlocale("fr_CA.UTF-8");
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({
    expression: 'inspector.breakProgram(\'\', JSON.stringify({a: 0.5}))'});
  let message = await Protocol.Debugger.oncePaused();
  InspectorTest.logObject(message.params.data || {});
  Protocol.Debugger.resume();

  Protocol.Runtime.evaluate({
    expression: 'inspector.breakProgram(\'\', JSON.stringify({a: 1}))'});
  message = await Protocol.Debugger.oncePaused();
  InspectorTest.logObject(message.params.data || {});
  Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();
