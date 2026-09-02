'use strict';

require('../common');
const assert = require('assert');
const {
  PerformanceResourceTiming,
  performance,
} = require('perf_hooks');

// Covers the IDL attributes finalResponseHeadersStart,
// firstInterimResponseStart, renderBlockingStatus, contentType and contentEncoding,
// and the spec requirement that responseStart prefers a non-zero
// firstInterimResponseStart over finalResponseHeadersStart.

function createTimingInfo(overrides = {}) {
  return {
    startTime: 0,
    redirectStartTime: 0,
    redirectEndTime: 0,
    postRedirectStartTime: 0,
    finalServiceWorkerStartTime: 0,
    finalNetworkRequestStartTime: 0,
    finalNetworkResponseStartTime: 0,
    endTime: 0,
    encodedBodySize: 0,
    decodedBodySize: 0,
    finalConnectionTimingInfo: null,
    ...overrides,
  };
}

function markResourceTiming(timingInfo, bodyInfo) {
  return performance.markResourceTiming(
    timingInfo,
    'http://localhost:8080',
    'fetch',
    {},
    '',
    bodyInfo,
    200,
    '',
  );
}

// Default values when no optional timing or body metadata is present.
{
  const resource = markResourceTiming(createTimingInfo(), {});

  assert.strictEqual(resource.finalResponseHeadersStart, 0);
  assert.strictEqual(resource.firstInterimResponseStart, 0);
  assert.strictEqual(resource.responseStart, 0);
  assert.strictEqual(resource.renderBlockingStatus, 'non-blocking');
  assert.strictEqual(resource.contentType, '');
  assert.strictEqual(resource.contentEncoding, '');
}

// responseStart falls back to the final response when no interim response timing was recorded.
{
  const resource = markResourceTiming(createTimingInfo({
    finalNetworkResponseStartTime: 123,
    firstInterimNetworkResponseStartTime: 0,
  }), {});

  assert.strictEqual(resource.finalResponseHeadersStart, 123);
  assert.strictEqual(resource.firstInterimResponseStart, 0);
  assert.strictEqual(resource.responseStart, 123);
}

// Values reflected from timing info and body info.
{
  const timingInfo = createTimingInfo({
    finalNetworkResponseStartTime: 123,
    firstInterimNetworkResponseStartTime: 45,
    renderBlocking: true,
  });
  const bodyInfo = {
    contentType: 'text/html',
    contentEncoding: 'gzip',
  };
  const resource = markResourceTiming(timingInfo, bodyInfo);

  assert.strictEqual(resource.finalResponseHeadersStart, 123);
  assert.strictEqual(resource.firstInterimResponseStart, 45);
  assert.strictEqual(resource.responseStart, 45);
  assert.strictEqual(resource.renderBlockingStatus, 'blocking');
  assert.strictEqual(resource.contentType, 'text/html');
  assert.strictEqual(resource.contentEncoding, 'gzip');

  const json = resource.toJSON();
  assert.strictEqual(json.finalResponseHeadersStart, 123);
  assert.strictEqual(json.firstInterimResponseStart, 45);
  assert.strictEqual(json.responseStart, 45);
  assert.strictEqual(json.renderBlockingStatus, 'blocking');
  assert.strictEqual(json.contentType, 'text/html');
  assert.strictEqual(json.contentEncoding, 'gzip');
}

// The attributes are enumerable getters on the prototype and perform a
// brand check like the other PerformanceResourceTiming attributes.
for (const name of [
  'finalResponseHeadersStart',
  'firstInterimResponseStart',
  'renderBlockingStatus',
  'contentType',
  'contentEncoding',
]) {
  const desc = Object.getOwnPropertyDescriptor(
    PerformanceResourceTiming.prototype, name);
  assert.strictEqual(desc.enumerable, true, name);
  assert.strictEqual(typeof desc.get, 'function', name);
  assert.throws(() => desc.get.call({}), {
    code: 'ERR_INVALID_THIS',
  }, name);
}

performance.clearResourceTimings();
