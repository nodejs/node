// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Runtime.evaluate returns object with undefined property.');

(async function test() {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "(function* f() {  yield f;})()",
    generatePreview: true
  }));
  InspectorTest.completeTest();
})();
