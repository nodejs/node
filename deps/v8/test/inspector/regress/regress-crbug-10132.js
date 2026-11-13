// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --print-break-location

// Ensure that --print-break-location correctly collects source positions on
// breaking.
let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests --print-break-location on debugger statement.');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(() => Protocol.Debugger.resume());
  await Protocol.Runtime.evaluate({expression: '\ndebugger;'});
  InspectorTest.completeTest();
})();
