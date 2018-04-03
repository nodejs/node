// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sampling-heap-profiler-suppress-randomness

(async function() {
  let {contextGroup, Protocol} = InspectorTest.start('Checks sampling heap profiler methods.');

  contextGroup.addScript(`
    var holder = [];
    function allocateChunk() {
      holder.push(new Array(100000).fill(42));
    }
    //# sourceURL=test.js`);

  Protocol.HeapProfiler.enable();

  const profile0 = await Protocol.HeapProfiler.getSamplingProfile();
  InspectorTest.log('Expected error: ' + profile0.error.message);

  await Protocol.HeapProfiler.startSampling();
  const profile1 = await Protocol.HeapProfiler.getSamplingProfile();
  const size1 = nodeSize(profile1.result.profile.head);
  InspectorTest.log('Allocated size is zero in the beginning:', size1 === 0);

  await Protocol.Runtime.evaluate({ expression: 'allocateChunk()' });
  const profile2 = await Protocol.HeapProfiler.getSamplingProfile();
  const size2 = nodeSize(profile2.result.profile.head);
  InspectorTest.log('Allocated size is more than 100KB after a chunk is allocated:', size2 > 100000);

  await Protocol.Runtime.evaluate({ expression: 'allocateChunk()' });
  const profile3 = await Protocol.HeapProfiler.getSamplingProfile();
  const size3 = nodeSize(profile3.result.profile.head);
  InspectorTest.log('Allocated size increased after one more chunk is allocated:', size3 > size2);

  const profile4 = await Protocol.HeapProfiler.stopSampling();
  const size4 = nodeSize(profile4.result.profile.head);
  InspectorTest.log('Allocated size did not change after stopping:', size4 === size3);

  InspectorTest.log('Successfully finished');
  InspectorTest.completeTest();

  function nodeSize(node) {
    return node.children.reduce((res, child) => res + nodeSize(child),
                                node.callFrame.functionName === 'allocateChunk' ? node.selfSize : 0);
  }
})();
