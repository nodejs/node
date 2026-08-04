// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sampling-heap-profiler-suppress-randomness

(async function() {
  let {contextGroup, Protocol} = InspectorTest.start('Checks call stack capturing by heap profiler.');

  contextGroup.addScript(`
    var holder = [];
    function allocateChunk() {
      holder.push(new Array(100000).fill(42));
    }
    function entry() { level1(); }
    function level1() { level2(); }
    function level2() { level3(); }
    function level3() { allocateChunk(); }
    //# sourceURL=test.js`);

  Protocol.HeapProfiler.enable();

  // should capture the full call stack by default
  const profile1 = await captureProfilerWithDepth();
  const size1 = callTreeSize(profile1.result.profile.head);
  InspectorTest.log('Call tree size:', size1);
  InspectorTest.log('Call tree contains allocationChunk:', hasNodeWithName(profile1.result.profile.head, 'allocateChunk'));

  // should capture the top call stack only with stackDepth=1
  const profile2 = await captureProfilerWithDepth(1);
  const size2 = callTreeSize(profile2.result.profile.head);
  InspectorTest.log('Call tree size:', size2);
  InspectorTest.log('Call tree contains allocationChunk:', hasNodeWithName(profile2.result.profile.head, 'allocateChunk'));

  // should fail with stackDepth=0
  await captureProfilerWithDepth(0);

  InspectorTest.log('Successfully finished');
  InspectorTest.completeTest();

  function callTreeSize(node) {
    return node.children.reduce((res, child) => res + callTreeSize(child), 1);
  }

  async function captureProfilerWithDepth(stackDepth) {
      try {
        InspectorTest.log('Starting heap profiler with stack depth: ' + (typeof stackDepth === 'number' ? stackDepth : '(unspecified)'));
        const res = await Protocol.HeapProfiler.startSampling(typeof stackDepth === 'number' ? { stackDepth } : {});

        if (res.error) {
          InspectorTest.log('Expected error: ' + res.error.message);
          return;
        }

        await Protocol.Runtime.evaluate({ expression: 'entry()' });
        return await Protocol.HeapProfiler.getSamplingProfile();
      } finally {
        await Protocol.HeapProfiler.stopSampling();
      }
  }

  function hasNodeWithName(node, name) {
    return node.callFrame.functionName === name || node.children.some(child => hasNodeWithName(child, name));
  }
})();
