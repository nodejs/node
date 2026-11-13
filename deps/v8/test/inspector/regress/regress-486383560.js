// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for issue 486383560.');

const source = `
function f() {
}

if (typeof d8 !== 'undefined' && d8.profiler) {
  d8.profiler.setOnProfileEndListener(f);
}
`;

(async function test() {
  await Protocol.Profiler.enable();
  contextGroup.addScript(source, 0, 0, 'test.js');
  await Protocol.Runtime.evaluate(
      {expression: 'console.profile("p"); console.profileEnd("p");'});
  await Protocol.Profiler.disable();
  InspectorTest.log('Test completed successfully.');
  InspectorTest.completeTest();
})();
