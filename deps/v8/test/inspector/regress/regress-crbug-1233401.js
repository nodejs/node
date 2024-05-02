// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sparkplug
// Flags: --no-baseline-batch-compilation

const baseScript = `
var paths = [
  [ 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 1],
  [ 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 1],
  [ 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 0]
];

function sink(arr) {
  let i = arr.length - 2;
  for (let j = 0; j < arr[i].length; j++) {
    const node = arr[i][j];
    const right = arr[i + 1][j + 1];
    arr[i][j] = Math.max(node + right);
  }
  arr.pop();
  return arr[i][arr[i].length-1];
}
`;

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Evaluate with and without side effect checks');

(async function test() {
  await Protocol.Runtime.evaluate({
    expression: baseScript,
    replMode: true,
  });
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: 'sink(paths);',
    replMode: true,
    throwOnSideEffect: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: 'sink(paths)',
    replMode: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: 'sink(paths);',
    replMode: true,
    throwOnSideEffect: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: 'sink(paths);',
    replMode: true,
  }));
  InspectorTest.completeTest();
})();
