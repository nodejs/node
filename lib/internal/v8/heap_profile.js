'use strict';

const {
  validateBoolean,
  validateInteger,
  validateInt32,
  validateObject,
} = require('internal/validators');

const {
  kSamplingNoFlags,
  kSamplingForceGC,
  kSamplingIncludeObjectsCollectedByMajorGC,
  kSamplingIncludeObjectsCollectedByMinorGC,
} = internalBinding('v8');

function normalizeHeapProfileOptions(options = {}) {
  validateObject(options, 'options');
  const {
    sampleInterval = 512 * 1024,
    stackDepth = 16,
    forceGC = false,
    includeObjectsCollectedByMajorGC = false,
    includeObjectsCollectedByMinorGC = false,
  } = options;

  validateInteger(sampleInterval, 'options.sampleInterval', 1);
  validateInt32(stackDepth, 'options.stackDepth', 0);
  validateBoolean(forceGC, 'options.forceGC');
  validateBoolean(includeObjectsCollectedByMajorGC,
                  'options.includeObjectsCollectedByMajorGC');
  validateBoolean(includeObjectsCollectedByMinorGC,
                  'options.includeObjectsCollectedByMinorGC');

  let flags = kSamplingNoFlags;
  if (forceGC) flags |= kSamplingForceGC;
  if (includeObjectsCollectedByMajorGC) {
    flags |= kSamplingIncludeObjectsCollectedByMajorGC;
  }
  if (includeObjectsCollectedByMinorGC) {
    flags |= kSamplingIncludeObjectsCollectedByMinorGC;
  }

  return { sampleInterval, stackDepth, flags };
}

module.exports = {
  normalizeHeapProfileOptions,
};
