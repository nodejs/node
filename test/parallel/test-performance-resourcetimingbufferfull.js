'use strict';

const common = require('../common');
const assert = require('assert');

const { performance } = require('perf_hooks');

function createTimingInfo(startTime) {
  const timingInfo = {
    startTime: startTime,
    endTime: startTime,
    finalServiceWorkerStartTime: 0,
    redirectStartTime: 0,
    redirectEndTime: 0,
    postRedirectStartTime: 0,
    finalConnectionTimingInfo: {
      domainLookupStartTime: 0,
      domainLookupEndTime: 0,
      connectionStartTime: 0,
      connectionEndTime: 0,
      secureConnectionStartTime: 0,
      ALPNNegotiatedProtocol: 0,
    },
    finalNetworkRequestStartTime: 0,
    finalNetworkResponseStartTime: 0,
    encodedBodySize: 0,
    decodedBodySize: 0,
  };
  return timingInfo;
}
const requestedUrl = 'https://nodejs.org';
const initiatorType = '';
const cacheMode = '';

async function main() {
  performance.setResourceTimingBufferSize(1);
  performance.markResourceTiming(createTimingInfo(1), requestedUrl, initiatorType, globalThis, cacheMode);
  // Trigger a resourcetimingbufferfull event.
  performance.markResourceTiming(createTimingInfo(2), requestedUrl, initiatorType, globalThis, cacheMode);
  performance.markResourceTiming(createTimingInfo(3), requestedUrl, initiatorType, globalThis, cacheMode);
  assert.strictEqual(performance.getEntriesByType('resource').length, 1);

  // Clear resource timings on resourcetimingbufferfull event.
  await new Promise((resolve) => {
    const listener = common.mustCall((event) => {
      assert.strictEqual(event.type, 'resourcetimingbufferfull');
      performance.removeEventListener('resourcetimingbufferfull', listener);

      performance.clearResourceTimings();
      assert.strictEqual(performance.getEntriesByType('resource').length, 0);

      resolve();
    });
    performance.addEventListener('resourcetimingbufferfull', listener);
  });

  // Secondary buffer has been added to the global buffer.
  {
    const entries = performance.getEntriesByType('resource');
    assert.strictEqual(entries.length, 1);
    assert.strictEqual(entries[0].startTime, 2);
    // The last item is discarded.
  }


  performance.clearResourceTimings();
  performance.setResourceTimingBufferSize(1);
  performance.markResourceTiming(createTimingInfo(4), requestedUrl, initiatorType, globalThis, cacheMode);
  // Trigger a resourcetimingbufferfull event.
  performance.markResourceTiming(createTimingInfo(5), requestedUrl, initiatorType, globalThis, cacheMode);
  performance.markResourceTiming(createTimingInfo(6), requestedUrl, initiatorType, globalThis, cacheMode);

  // Increase the buffer size on resourcetimingbufferfull event.
  await new Promise((resolve) => {
    const listener = common.mustCall((event) => {
      assert.strictEqual(event.type, 'resourcetimingbufferfull');
      performance.removeEventListener('resourcetimingbufferfull', listener);

      performance.setResourceTimingBufferSize(2);
      assert.strictEqual(performance.getEntriesByType('resource').length, 1);

      resolve();
    });
    performance.addEventListener('resourcetimingbufferfull', listener);
  });

  // Secondary buffer has been added to the global buffer.
  {
    const entries = performance.getEntriesByType('resource');
    assert.strictEqual(entries.length, 2);
    assert.strictEqual(entries[0].startTime, 4);
    assert.strictEqual(entries[1].startTime, 5);
    // The last item is discarded.
  }


  performance.clearResourceTimings();
  performance.setResourceTimingBufferSize(2);
  performance.markResourceTiming(createTimingInfo(7), requestedUrl, initiatorType, globalThis, cacheMode);
  performance.markResourceTiming(createTimingInfo(8), requestedUrl, initiatorType, globalThis, cacheMode);
  // Trigger a resourcetimingbufferfull event.
  performance.markResourceTiming(createTimingInfo(9), requestedUrl, initiatorType, globalThis, cacheMode);

  // Decrease the buffer size on resourcetimingbufferfull event.
  await new Promise((resolve) => {
    const listener = common.mustCall((event) => {
      assert.strictEqual(event.type, 'resourcetimingbufferfull');
      performance.removeEventListener('resourcetimingbufferfull', listener);

      performance.setResourceTimingBufferSize(1);
      assert.strictEqual(performance.getEntriesByType('resource').length, 2);

      resolve();
    });
    performance.addEventListener('resourcetimingbufferfull', listener);
  });

  // Secondary buffer has been added to the global buffer.
  {
    const entries = performance.getEntriesByType('resource');
    assert.strictEqual(entries.length, 2);
    assert.strictEqual(entries[0].startTime, 7);
    assert.strictEqual(entries[1].startTime, 8);
    // The last item is discarded.
  }
}

main();
