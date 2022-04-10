'use strict';
// https://developer.mozilla.org/en-US/docs/Web/API/PerformanceResourceTiming

const { InternalPerformanceEntry } = require('internal/perf/performance_entry');
const { SymbolToStringTag } = primordials;
const assert = require('internal/assert');
const { enqueue } = require('internal/perf/observe');
const { Symbol, ObjectSetPrototypeOf } = primordials;

const kCacheMode = Symbol('kCacheMode');
const kRequestedUrl = Symbol('kRequestedUrl');
const kTimingInfo = Symbol('kTimingInfo');
const kInitiatorType = Symbol('kInitiatorType');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
  }
} = require('internal/errors');

class InternalPerformanceResourceTiming extends InternalPerformanceEntry {
  constructor(requestedUrl, initiatorType, timingInfo, cacheMode = '') {
    super(requestedUrl, 'resource');
    this[kInitiatorType] = initiatorType;
    this[kRequestedUrl] = requestedUrl;
    // https://fetch.spec.whatwg.org/#fetch-timing-info
    // This class is using timingInfo assuming it's already validated.
    // The spec doesn't say to validate it in the class construction.
    this[kTimingInfo] = timingInfo;
    this[kCacheMode] = cacheMode;
  }

  get [SymbolToStringTag]() {
    return 'PerformanceResourceTiming';
  }

  get name() {
    return this[kRequestedUrl];
  }

  get startTime() {
    return this[kTimingInfo].startTime;
  }

  get duration() {
    return this[kTimingInfo].endTime - this[kTimingInfo].startTime;
  }

  get workerStart() {
    return this[kTimingInfo].finalServiceWorkerStartTime;
  }

  get redirectStart() {
    return this[kTimingInfo].redirectStartTime;
  }

  get redirectEnd() {
    return this[kTimingInfo].redirectEndTime;
  }

  get fetchStart() {
    return this[kTimingInfo].postRedirectStartTime;
  }

  get domainLookupStart() {
    return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupStartTime;
  }

  get domainLookupEnd() {
    return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupEndTime;
  }

  get connectStart() {
    return this[kTimingInfo].finalConnectionTimingInfo?.connectionStartTime;
  }

  get connectEnd() {
    return this[kTimingInfo].finalConnectionTimingInfo?.connectionEndTime;
  }

  get secureConnectionStart() {
    return this[kTimingInfo]
      .finalConnectionTimingInfo?.secureConnectionStartTime;
  }

  get nextHopProtocol() {
    return this[kTimingInfo]
      .finalConnectionTimingInfo?.ALPNNegotiatedProtocol;
  }

  get requestStart() {
    return this[kTimingInfo].finalNetworkRequestStartTime;
  }

  get responseStart() {
    return this[kTimingInfo].finalNetworkResponseStartTime;
  }

  get responseEnd() {
    return this[kTimingInfo].endTime;
  }

  get encodedBodySize() {
    return this[kTimingInfo].encodedBodySize;
  }

  get decodedBodySize() {
    return this[kTimingInfo].decodedBodySize;
  }

  get transferSize() {
    if (this[kCacheMode] === 'local') return 0;
    if (this[kCacheMode] === 'validated') return 300;

    return this[kTimingInfo].encodedBodySize + 300;
  }

  toJSON() {
    return {
      name: this.name,
      entryType: this.entryType,
      startTime: this.startTime,
      duration: this.duration,
      initiatorType: this[kInitiatorType],
      nextHopProtocol: this.nextHopProtocol,
      workerStart: this.workerStart,
      redirectStart: this.redirectStart,
      redirectEnd: this.redirectEnd,
      fetchStart: this.fetchStart,
      domainLookupStart: this.domainLookupStart,
      domainLookupEnd: this.domainLookupEnd,
      connectStart: this.connectStart,
      connectEnd: this.connectEnd,
      secureConnectionStart: this.secureConnectionStart,
      requestStart: this.requestStart,
      responseStart: this.responseStart,
      responseEnd: this.responseEnd,
      transferSize: this.transferSize,
      encodedBodySize: this.encodedBodySize,
      decodedBodySize: this.decodedBodySize,
    };
  }
}

class PerformanceResourceTiming extends InternalPerformanceResourceTiming {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }
}

// https://w3c.github.io/resource-timing/#dfn-mark-resource-timing
function markResourceTiming(
  timingInfo,
  requestedUrl,
  initiatorType,
  global,
  cacheMode,
) {
  // https://w3c.github.io/resource-timing/#dfn-setup-the-resource-timing-entry
  assert(
    cacheMode === '' || cacheMode === 'local',
    'cache must be an empty string or \'local\'',
  );
  const resource = new InternalPerformanceResourceTiming(
    requestedUrl,
    initiatorType,
    timingInfo,
    cacheMode,
  );

  ObjectSetPrototypeOf(resource, PerformanceResourceTiming.prototype);
  enqueue(resource);
  return resource;
}

module.exports = {
  PerformanceResourceTiming,
  markResourceTiming,
};
