'use strict';
// https://developer.mozilla.org/en-US/docs/Web/API/PerformanceResourceTiming

const {
  ObjectDefineProperties,
  Symbol,
  SymbolToStringTag,
} = primordials;
const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
  },
} = require('internal/errors');
const { PerformanceEntry, kSkipThrow } = require('internal/perf/performance_entry');
const assert = require('internal/assert');
const { enqueue, bufferResourceTiming } = require('internal/perf/observe');
const { validateThisInternalField } = require('internal/validators');
const { kEnumerableProperty } = require('internal/util');

const kCacheMode = Symbol('kCacheMode');
const kRequestedUrl = Symbol('kRequestedUrl');
const kTimingInfo = Symbol('kTimingInfo');
const kInitiatorType = Symbol('kInitiatorType');
const kDeliveryType = Symbol('kDeliveryType');
const kResponseStatus = Symbol('kResponseStatus');

class PerformanceResourceTiming extends PerformanceEntry {
  constructor(skipThrowSymbol = undefined, name = undefined, type = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    super(skipThrowSymbol, name, type);
  }

  get name() {
    validateThisInternalField(this, kRequestedUrl, 'PerformanceResourceTiming');
    return this[kRequestedUrl];
  }

  get startTime() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].startTime;
  }

  get duration() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].endTime - this[kTimingInfo].startTime;
  }

  get initiatorType() {
    validateThisInternalField(this, kInitiatorType, 'PerformanceResourceTiming');
    return this[kInitiatorType];
  }

  get workerStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalServiceWorkerStartTime;
  }

  get redirectStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].redirectStartTime;
  }

  get redirectEnd() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].redirectEndTime;
  }

  get fetchStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].postRedirectStartTime;
  }

  get domainLookupStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupStartTime;
  }

  get domainLookupEnd() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupEndTime;
  }

  get connectStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.connectionStartTime;
  }

  get connectEnd() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalConnectionTimingInfo?.connectionEndTime;
  }

  get secureConnectionStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo]
      .finalConnectionTimingInfo?.secureConnectionStartTime;
  }

  get nextHopProtocol() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo]
      .finalConnectionTimingInfo?.ALPNNegotiatedProtocol;
  }

  get requestStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalNetworkRequestStartTime;
  }

  get responseStart() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].finalNetworkResponseStartTime;
  }

  get responseEnd() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].endTime;
  }

  get encodedBodySize() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].encodedBodySize;
  }

  get decodedBodySize() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kTimingInfo].decodedBodySize;
  }

  get transferSize() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    if (this[kCacheMode] === 'local') return 0;
    if (this[kCacheMode] === 'validated') return 300;

    return this[kTimingInfo].encodedBodySize + 300;
  }

  get deliveryType() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kDeliveryType];
  }

  get responseStatus() {
    validateThisInternalField(this, kTimingInfo, 'PerformanceResourceTiming');
    return this[kResponseStatus];
  }

  toJSON() {
    validateThisInternalField(this, kInitiatorType, 'PerformanceResourceTiming');
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
      deliveryType: this.deliveryType,
      responseStatus: this.responseStatus,
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
  deliveryType: kEnumerableProperty,
  responseStatus: kEnumerableProperty,
  toJSON: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'PerformanceResourceTiming',
  },
});

function createPerformanceResourceTiming(
  requestedUrl,
  initiatorType,
  timingInfo,
  cacheMode = '',
  bodyInfo,
  responseStatus,
  deliveryType,
) {
  const resourceTiming = new PerformanceResourceTiming(kSkipThrow, requestedUrl, 'resource');

  resourceTiming[kInitiatorType] = initiatorType;
  resourceTiming[kRequestedUrl] = requestedUrl;
  // https://fetch.spec.whatwg.org/#fetch-timing-info
  // This class is using timingInfo assuming it's already validated.
  // The spec doesn't say to validate it in the class construction.
  resourceTiming[kTimingInfo] = timingInfo;
  resourceTiming[kCacheMode] = cacheMode;
  resourceTiming[kDeliveryType] = deliveryType;
  resourceTiming[kResponseStatus] = responseStatus;

  return resourceTiming;
}

// https://w3c.github.io/resource-timing/#dfn-mark-resource-timing
function markResourceTiming(
  timingInfo,
  requestedUrl,
  initiatorType,
  global,
  cacheMode,
  bodyInfo,
  responseStatus,
  deliveryType = '',
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
    bodyInfo,
    responseStatus,
    deliveryType,
  );

  enqueue(resource);
  bufferResourceTiming(resource);
  return resource;
}

module.exports = {
  PerformanceResourceTiming,
  markResourceTiming,
};
