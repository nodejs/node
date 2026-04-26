'use strict';

const {
  MathFloor,
} = primordials;

const {
  validateNumber,
  validateObject,
} = require('internal/validators');

const kMicrosPerMilli = 1_000;
const kMaxSamplingIntervalUs = 0x7FFFFFFF;
const kMaxSamplingIntervalMs = kMaxSamplingIntervalUs / kMicrosPerMilli;
const kMaxSamplesUnlimited = 0xFFFF_FFFF;

function normalizeCpuProfileOptions(options = {}) {
  validateObject(options, 'options');

  // TODO(ishabi): add support for 'mode' and 'filterContext' options
  const {
    sampleInterval,
    maxBufferSize,
  } = options;

  let samplingIntervalMicros = 0;
  if (sampleInterval !== undefined) {
    validateNumber(sampleInterval,
                   'options.sampleInterval',
                   0,
                   kMaxSamplingIntervalMs);
    samplingIntervalMicros = MathFloor(sampleInterval * kMicrosPerMilli);
    if (sampleInterval > 0 && samplingIntervalMicros === 0) {
      samplingIntervalMicros = 1;
    }
  }

  const size = maxBufferSize;
  let normalizedMaxSamples = kMaxSamplesUnlimited;
  if (size !== undefined) {
    validateNumber(size,
                   'options.maxBufferSize',
                   1,
                   kMaxSamplesUnlimited);
    normalizedMaxSamples = MathFloor(size);
  }

  return {
    samplingIntervalMicros,
    maxSamples: normalizedMaxSamples,
  };
}

module.exports = {
  normalizeCpuProfileOptions,
};
