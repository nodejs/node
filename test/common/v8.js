'use strict';
const assert = require('assert');
const { GCProfiler } = require('v8');

function collectGCProfile({ duration, mode, format }) {
  return new Promise((resolve) => {
    const profiler = new GCProfiler({
      mode,
    });
    profiler.start();
    setTimeout(() => {
      resolve(profiler.stop(format));
    }, duration);
  });
}

function checkGCProfile(data, options) {
  if (options?.format === GCProfiler.GC_PROFILER_FORMAT.NONE) {
    assert.ok(data === undefined);
    return;
  }
  if (options?.format === GCProfiler.GC_PROFILER_FORMAT.STRING) {
    data = JSON.parse(data);
  }
  assert.ok(data.version > 0);
  assert.ok(data.startTime >= 0);
  assert.ok(data.endTime >= 0);
  assert.ok(data.totalGCTime >= 0);
  if (options?.mode === GCProfiler.GC_PROFILER_MODE.TIME) {
    assert.ok(data.statistics === undefined);
    return;
  }
  assert.ok(Array.isArray(data.statistics));
  // If the array is not empty, check it
  if (data.statistics.length) {
    // Just check the first one
    const item = data.statistics[0];
    assert.ok(typeof item.gcType === 'string');
    assert.ok(item.cost >= 0);
    assert.ok(typeof item.beforeGC === 'object');
    assert.ok(typeof item.afterGC === 'object');
    // The content of beforeGC and afterGC is same, so we just check afterGC
    assert.ok(typeof item.afterGC.heapStatistics === 'object');
    const heapStatisticsKeys = [
      'externalMemory',
      'heapSizeLimit',
      'mallocedMemory',
      'peakMallocedMemory',
      'totalAvailableSize',
      'totalGlobalHandlesSize',
      'totalHeapSize',
      'totalHeapSizeExecutable',
      'totalPhysicalSize',
      'usedGlobalHandlesSize',
      'usedHeapSize',
    ];
    heapStatisticsKeys.forEach((key) => {
      assert.ok(item.afterGC.heapStatistics[key] >= 0);
    });
    assert.ok(typeof item.afterGC.heapSpaceStatistics === 'object');
    const heapSpaceStatisticsKeys = [
      'physicalSpaceSize',
      'spaceAvailableSize',
      'spaceName',
      'spaceSize',
      'spaceUsedSize',
    ];
    heapSpaceStatisticsKeys.forEach((key) => {
      const value = item.afterGC.heapSpaceStatistics[0][key];
      assert.ok(key === 'spaceName' ? typeof value === 'string' : value >= 0);
    });
  }
}

async function testGCProfiler(options) {
  const data = await collectGCProfile({ duration: 5000, ...options });
  checkGCProfile(data, options);
}

function emitGC() {
  for (let i = 0; i < 100; i++) {
    new Array(100);
  }
  global?.gc();
}

module.exports = {
  collectGCProfile,
  checkGCProfile,
  testGCProfiler,
  emitGC,
};
