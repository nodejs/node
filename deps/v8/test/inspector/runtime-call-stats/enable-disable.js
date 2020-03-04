// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test RunTimeCallStats collection enabling and disabling.');

var source =
`
function fib(x) {
  if (x < 2) return 1;
  return fib(x-1) + fib(x-2);
}
fib(5);
`;

function logErrorMessage(result) {
  InspectorTest.log('Expected error: "' + result.error.message + '"');
}

(async function test() {
  await Protocol.Runtime.ensable();

  // This should fail with "not enabled" error.
  logErrorMessage(await Protocol.Profiler.getRuntimeCallStats());

  // This should fail with "already enabled" error.
  await Protocol.Profiler.enableRuntimeCallStats();
  logErrorMessage(await Protocol.Profiler.enableRuntimeCallStats());

  // The result should not be empty.
  await Protocol.Runtime.evaluate({ expression: source, sourceURL: arguments.callee.name, persistScript: true });
  const counters = (await Protocol.Profiler.getRuntimeCallStats()).result.result;
  if (counters.length > 0)
    InspectorTest.log('Some counters reported');
  await Protocol.Profiler.disableRuntimeCallStats();

  // This should fail with "not enabled" error too.
  logErrorMessage(await Protocol.Profiler.getRuntimeCallStats());

  // The result should not be empty and have smaller amount of counters than
  // the first result.
  await Protocol.Profiler.enableRuntimeCallStats();
  const counters2 = (await Protocol.Profiler.getRuntimeCallStats()).result.result;
  if (counters2.length > 0 && counters2.length < counters.length)
    InspectorTest.log('Less counters reported');
  await Protocol.Profiler.disableRuntimeCallStats();

  await Protocol.Runtime.disable();
  InspectorTest.completeTest();
})().catch(e => InspectorTest.log('caught: ' + e));
