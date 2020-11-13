// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test RunTimeCallStats collection using Profiler.getRuntimeCallStats.');

var source =
`
function fib(x) {
  if (x < 2) return 1;
  return fib(x-1) + fib(x-2);
}
fib(5);
`;

function buildCounterMap(result) {
  let counterMap = new Map();

  let counters = result.result.result;
  for (const {name, value} of counters) {
    counterMap.set(name, value);
  }

  return counterMap;
}

function compareCounterMaps(counterMap, counterMap2) {
  // Check for counters that are present in the first map but are not found
  // in the the second map
  for (let counter of counterMap.keys()) {
    if (!counterMap2.has(counter)) {
      InspectorTest.log(`Counter ${counter} is missing`);
      return false;
    }
  }

  // Check for the counter value changes
  let counterValueIncreased = false;
  for (let [counter, value2] of counterMap2) {
    let value = counterMap.get(counter);
    if (value !== undefined) {
      if (value2 < value) {
        InspectorTest.log(`Counter ${counter} value decreased: ${value} -> ${value2}`);
        return false;
      }
      if (value2 > value) {
        counterValueIncreased = true;
      }
    }
  }

  if (!counterValueIncreased && counterMap.size === counterMap2.size) {
    InspectorTest.log(`No counter values has increased or added`);
    return false;
  }

  return true;
}

(async function test() {
  await Protocol.Runtime.enable();
  await Protocol.Profiler.enableRuntimeCallStats();

  let counterMap = buildCounterMap(await Protocol.Profiler.getRuntimeCallStats());

  await Protocol.Runtime.evaluate({ expression: source, sourceURL: arguments.callee.name, persistScript: true });

  let counterMap2 = buildCounterMap(await Protocol.Profiler.getRuntimeCallStats());
  const check1 = compareCounterMaps(counterMap, counterMap2);

  await Protocol.Runtime.evaluate({ expression: source, sourceURL: arguments.callee.name, persistScript: true });

  let counterMap3 = buildCounterMap(await Protocol.Profiler.getRuntimeCallStats());
  const check2 = compareCounterMaps(counterMap2, counterMap3);

  await Protocol.Profiler.disableRuntimeCallStats();
  await Protocol.Runtime.disable();

  InspectorTest.log(check1 && check2 ? 'PASSED' : 'FAILED');

  InspectorTest.completeTest();
})().catch(e => InspectorTest.log('caught: ' + e));
