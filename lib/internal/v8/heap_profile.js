'use strict';

const {
  ObjectFreeze,
} = primordials;

const {
  validateInteger,
  validateInt32,
} = require('internal/validators');

const {
  kSamplingNoFlags,
  kSamplingForceGC,
  kSamplingIncludeObjectsCollectedByMajorGC,
  kSamplingIncludeObjectsCollectedByMinorGC,
} = internalBinding('v8');

const heapProfilerConstants = {
  __proto__: null,
  SAMPLING_NO_FLAGS: kSamplingNoFlags,
  SAMPLING_FORCE_GC: kSamplingForceGC,
  SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MAJOR_GC:
    kSamplingIncludeObjectsCollectedByMajorGC,
  SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MINOR_GC:
    kSamplingIncludeObjectsCollectedByMinorGC,
};
ObjectFreeze(heapProfilerConstants);

const kMaxSamplingFlags =
  heapProfilerConstants.SAMPLING_FORCE_GC |
  heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MAJOR_GC |
  heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MINOR_GC;

function normalizeHeapProfileOptions(
  sampleInterval = 512 * 1024,
  stackDepth = 16,
  flags = heapProfilerConstants.SAMPLING_NO_FLAGS,
) {
  validateInteger(sampleInterval, 'sampleInterval', 1);
  validateInt32(stackDepth, 'stackDepth', 0);
  validateInt32(flags, 'flags', 0, kMaxSamplingFlags);
  return { sampleInterval, stackDepth, flags };
}

module.exports = {
  heapProfilerConstants,
  normalizeHeapProfileOptions,
};
