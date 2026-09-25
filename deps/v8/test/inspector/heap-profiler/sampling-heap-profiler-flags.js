// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sampling-heap-profiler-suppress-randomness
// Flags: --no-stress-incremental-marking
// Flags: --allow-natives-syntax

(async function() {
  let {contextGroup, Protocol} = InspectorTest.start('Checks sampling heap profiler methods.');

  contextGroup.addScript(`
    function generateTrash() {
      var arr = new Array(100);
      for (var i = 0; i < 3000; ++i) {
        var s = {a:i, b: new Array(100).fill(42)};
        arr[i % 100] = s;
      }
      return arr[30];
    }
    %PrepareFunctionForOptimization(generateTrash);
    generateTrash();
    %OptimizeFunctionOnNextCall(generateTrash);
    generateTrash();
    //# sourceURL=test.js`);

  Protocol.HeapProfiler.enable();

  await Protocol.HeapProfiler.startSampling({
    samplingInterval: 1e4,
    includeObjectsCollectedByMajorGC: false,
    includeObjectsCollectedByMinorGC: false,
  });
  await Protocol.Runtime.evaluate({ expression: 'generateTrash()' });
  const profile1 = await Protocol.HeapProfiler.stopSampling();
  const size1 = nodeSize(profile1.result.profile.head);
  InspectorTest.log('Retained size is less than 15KB:', size1 < 15000);

  await Protocol.HeapProfiler.startSampling({
    samplingInterval: 100,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: false,
  });
  await Protocol.Runtime.evaluate({ expression: 'generateTrash()' });
  const profile2 = await Protocol.HeapProfiler.stopSampling();
  const size2 = nodeSize(profile2.result.profile.head);
  InspectorTest.log('Including major GC increases size:', size1 < size2);

  await Protocol.HeapProfiler.startSampling({
    samplingInterval: 100,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: true,
  });
  await Protocol.Runtime.evaluate({ expression: 'generateTrash()' });
  const profile3 = await Protocol.HeapProfiler.stopSampling();
  const size3 = nodeSize(profile3.result.profile.head);
  InspectorTest.log('Minor GC collected more:', size3 > size2);
  InspectorTest.log('Total allocation is greater than 100KB:', size3 > 100000);

  InspectorTest.log('Successfully finished');
  InspectorTest.completeTest();

  function nodeSize(node) {
    return node.children.reduce((res, child) => res + nodeSize(child),
        node.callFrame.functionName === 'generateTrash' ? node.selfSize : 0);
  }
})();
