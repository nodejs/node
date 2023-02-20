'use strict';
// https://developer.mozilla.org/en-US/docs/Web/API/PerformanceResourceTiming

const {
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  ReflectConstruct,
  Symbol,
  SymbolToStringTag,
} = primordials;
const { initPerformanceEntry, PerformanceEntry } = require('internal/perf/performance_entry');
const assert = require('internal/assert');
const { enqueue, bufferResourceTiming } = require('internal/perf/observe');
const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
  },
} = require('internal/errors');
const { validateInternalField } = require('internal/validators');
const { kEnumerableProperty } = require('internal/util');

const kCacheMode = Symbol('kCacheMode');
const kRequestedUrl = Symbol('kRequestedUrl');
const kTimingInfo = Symbol('kTimingInfo');
const kInitiatorType = Symbol('kInitiatorType');

class PerformanceResourceTiming extends PerformanceEntry {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  get name() {
    validateInternalField(this, kRequestedUrl, 'PerformanceResourceTiming');
    return this[kRequestedUrl];
  }

  get startTime() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].startTime;
  }

  get duration() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].endTime - this[kTimingInfo].startTime;
  }

  get initiatorType() {
    validateInternalField(this, kInitiatorType, 'PerformanceResourceTiming');
    return this[kInitiatorType];
  }

  get workerStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalServiceWorkerStartTime;
  }

  get redirectStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].redirectStartTime;
  }

  get redirectEnd() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].redirectEndTime;
  }

  get fetchStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].postRedirectStartTime;
  }

  get domainLookupStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupStartTime;
  }

  get domainLookupEnd() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupEndTime;
  }

  get connectStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.connectionStartTime;
  }

  get connectEnd() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.connectionEndTime;
  }

  get secureConnectionStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo]
      .finalConnectionTimingInfo?.secureConnectionStartTime;
  }

  get nextHopProtocol() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo]
      .finalConnectionTimingInfo?.ALPNNegotiatedProtocol;
  }

  get requestStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalNetworkRequestStartTime;
  }

  get responseStart() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalNetworkResponseStartTime;
  }

  get responseEnd() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].endTime;
  }

  get encodedBodySize() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].encodedBodySize;
  }

  get decodedBodySize() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].decodedBodySize;
  }

  get transferSize() {
    validateInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    if (this[kCacheMode] === 'local') return 0;
    if (this[kCacheMode] === 'validated') return 300;

    return this[kTimingInfo].encodedBodySize + 300;
  }

  toJSON() {
    validateInternalField(this, kInitiatorType, 'PerformanceResourceTiming');
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

ObjectDefineProperties(PerformanceResourceTiming.prototype, {
  initiatorType: kEnumerableProperty,
  nextHopProtocol: kEnumerableProperty,
  workerStart: kEnumerableProperty,
  redirectStart: kEnumerableProperty,
  redirectEnd: kEnumerableProperty,
  fetchStart: kEnumerableProperty,
  domainLookupStart: kEnumerableProperty,
  domainLookupEnd: kEnumerableProperty,
  connectStart: kEnumerableProperty,
  connectEnd: kEnumerableProperty,
  secureConnectionStart: kEnumerableProperty,
  requestStart: kEnumerableProperty,
  responseStart: kEnumerableProperty,
  responseEnd: kEnumerableProperty,
  transferSize: kEnumerableProperty,
  encodedBodySize: kEnumerableProperty,
  decodedBodySize: kEnumerableProperty,
  toJSON: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'PerformanceResourceTiming',
  },
});

function createPerformanceResourceTiming(requestedUrl, initiatorType, timingInfo, cacheMode = '') {
  return ReflectConstruct(function PerformanceResourceTiming() {
    initPerformanceEntry(this, requestedUrl, 'resource');
    this[kInitiatorType] = initiatorType;
    this[kRequestedUrl] = requestedUrl;
    // https://fetch.spec.whatwg.org/#fetch-timing-info
    // This class is using timingInfo assuming it's already validated.
    // The spec doesn't say to validate it in the class construction.
    this[kTimingInfo] = timingInfo;
    this[kCacheMode] = cacheMode;
  }, [], PerformanceResourceTiming);
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
  const resource = createPerformanceResourceTiming(
    requestedUrl,
    initiatorType,
    timingInfo,
    cacheMode,
  );

  ObjectSetPrototypeOf(resource, PerformanceResourceTiming.prototype);
  enqueue(resource);
  bufferResourceTiming(resource);
  return resource;
}

module.exports = {
  PerformanceResourceTiming,
  markResourceTiming,
};
