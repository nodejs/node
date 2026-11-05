// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-debug-context-load.');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.disable();
  InspectorTest.log('Done.');
  InspectorTest.completeTest();
})();
