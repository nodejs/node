// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test Counters collection enabling and disabling.');

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
  await Protocol.Runtime.enable();

  // This should fail with "not enabled" error.
  logErrorMessage(await Protocol.Profiler.getCounters());

  // This should fail with "already enabled" error.
  await Protocol.Profiler.enableCounters();
  logErrorMessage(await Protocol.Profiler.enableCounters());

  // The result should not be empty.
  await Protocol.Runtime.evaluate({ expression: source, sourceURL: arguments.callee.name, persistScript: true });
  const counters = (await Protocol.Profiler.getCounters()).result.result;
  if (counters.length > 0)
    InspectorTest.log('Some counters reported');
  await Protocol.Profiler.disableCounters();

  // This should fail with "not enabled" error too.
  logErrorMessage(await Protocol.Profiler.getCounters());

  // The result should not be empty and have smaller amount of counters than
  // the first result.
  await Protocol.Profiler.enableCounters();
  const counters2 = (await Protocol.Profiler.getCounters()).result.result;
  if (counters2.length > 0 && counters2.length < counters.length)
    InspectorTest.log('Less counters reported');
  await Protocol.Profiler.disableCounters();

  await Protocol.Runtime.disable();
  InspectorTest.completeTest();
})().catch(e => InspectorTest.log('caught: ' + e));
