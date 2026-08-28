'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeReverse,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  JSONStringify,
  MathFloor,
  MathMax,
  MathMin,
  MathRound,
  MathSqrt,
  Number,
  NumberIsFinite,
  NumberMAX_SAFE_INTEGER,
  ObjectFreeze,
  ObjectKeys,
  PromiseWithResolvers,
  SafeSet,
  StringPrototypeToLowerCase,
} = primordials;
const { AsyncResource } = require('async_hooks');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { createHistogram } = require('internal/histogram');
const { TIMEOUT_MAX } = require('internal/timers');
const { kEmptyObject } = require('internal/util');
const {
  validateAbortSignal,
  validateFunction,
  validateInteger,
  validateNumber,
  validateObject,
  validateString,
  validateUint32,
} = require('internal/validators');
const { structuredClone } = require('internal/worker/js_transferable');

const { bigint: hrtime } = process.hrtime;
const kDefaultSamples = 30;
const kDefaultWarmup = 0;
const kEmptyNamePath = ObjectFreeze([]);
const kEmptyParams = ObjectFreeze({ __proto__: null });
const kEmptyTags = ObjectFreeze([]);

function validateSkip(skip) {
  if (skip !== undefined && typeof skip !== 'boolean' &&
      typeof skip !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('options.skip', ['boolean', 'string'], skip);
  }
}

function canonicalizeTags(tags, parentTags = kEmptyTags) {
  if (tags === undefined) return parentTags;
  if (!ArrayIsArray(tags)) {
    throw new ERR_INVALID_ARG_TYPE('options.tags', 'Array', tags);
  }

  const result = ArrayPrototypeSlice(parentTags);
  const seen = new SafeSet(parentTags);
  for (let i = 0; i < tags.length; i++) {
    validateString(tags[i], `options.tags[${i}]`);
    if (tags[i].length === 0) {
      throw new ERR_INVALID_ARG_VALUE(
        `options.tags[${i}]`, tags[i], 'must not be empty');
    }
    const tag = StringPrototypeToLowerCase(tags[i]);
    if (!seen.has(tag)) {
      seen.add(tag);
      ArrayPrototypePush(result, tag);
    }
  }
  return ObjectFreeze(result);
}

function canonicalizeParams(params) {
  if (params === undefined) return kEmptyParams;
  validateObject(params, 'options.params');

  const result = { __proto__: null };
  const keys = ObjectKeys(params);
  ArrayPrototypeSort(keys);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const value = params[key];
    if (typeof value !== 'string' && typeof value !== 'boolean' &&
        (typeof value !== 'number' || !NumberIsFinite(value))) {
      if (typeof value === 'number') {
        throw new ERR_OUT_OF_RANGE(
          `options.params.${key}`, 'a finite number', value);
      }
      throw new ERR_INVALID_ARG_TYPE(
        `options.params.${key}`, ['string', 'number', 'boolean'], value);
    }
    result[key] = value;
  }
  return ObjectFreeze(result);
}

function validateNodeOptions(options, parentTags) {
  validateObject(options, 'options');
  const { only = false, skip, tags } = options;
  if (typeof only !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.only', 'boolean', only);
  }
  validateSkip(skip);
  return {
    __proto__: null,
    only,
    skip,
    tags: canonicalizeTags(tags, parentTags),
  };
}

function createLocation(loc, fallbackFile) {
  return {
    __proto__: null,
    file: loc?.[2] ?? fallbackFile,
    line: loc?.[0],
    column: loc?.[1],
  };
}

function getNamePath(parent, name) {
  const path = [];
  for (let current = parent; current?.parent !== null; current = current.parent) {
    ArrayPrototypePush(path, current.name);
  }
  ArrayPrototypeReverse(path);
  ArrayPrototypePush(path, name);
  return path;
}

function cloneSample(sample) {
  const result = {
    __proto__: null,
    operations: sample.operations,
    duration_ns: sample.duration_ns,
    rate: sample.rate,
  };
  if (sample.detail !== undefined) {
    result.detail = structuredClone(sample.detail);
  }
  return result;
}

class Suite extends AsyncResource {
  constructor(harness, parent, name, options, fn, loc, isRoot = false) {
    super('BenchSuite');
    const validated = validateNodeOptions(
      options, parent?.tags ?? kEmptyTags);

    this.harness = harness;
    this.parent = parent;
    this.name = name;
    this.fn = fn;
    this.loc = createLocation(loc, harness.entryFile);
    this.isRoot = isRoot;
    this.fileScope = isRoot ? null :
      (parent.isRoot ? harness.getFileScope() : parent.fileScope);
    this.namePath = isRoot ? kEmptyNamePath :
      ObjectFreeze(getNamePath(parent, name));
    this.suiteId = isRoot ? null : JSONStringify([
      this.loc.file,
      this.namePath,
    ]);
    this.parentId = isRoot || parent.isRoot ? null : parent.suiteId;
    this.only = validated.only;
    this.skip = validated.skip;
    this.tags = validated.tags;
    this.children = [];
    this.hooks = {
      __proto__: null,
      after: [],
      afterEach: [],
      before: [],
      beforeEach: [],
    };
    this.buildError = null;
    this.buildPromise = null;
    this.finished = false;
    this.completion = PromiseWithResolvers();
  }
}

class Bench extends AsyncResource {
  constructor(harness, parent, name, options, fn, loc) {
    super('Benchmark');
    const validated = validateNodeOptions(options, parent.tags);
    const {
      params,
      samples = kDefaultSamples,
      signal,
      timeout = Infinity,
      warmup = kDefaultWarmup,
    } = options;

    validateUint32(samples, 'options.samples', true);
    validateUint32(warmup, 'options.warmup');
    validateAbortSignal(signal, 'options.signal');
    if (timeout !== Infinity) {
      validateNumber(timeout, 'options.timeout', 0, TIMEOUT_MAX);
    }

    this.harness = harness;
    this.parent = parent;
    this.name = name;
    this.fn = fn;
    this.loc = createLocation(loc, harness.entryFile);
    this.only = validated.only;
    this.skip = validated.skip;
    this.tags = validated.tags;
    this.params = canonicalizeParams(params);
    this.samples = samples;
    this.warmup = warmup;
    this.timeout = timeout;
    this.outerSignal = signal;
    this.fileScope = parent.isRoot ? harness.getFileScope() : parent.fileScope;
    this.namePath = ObjectFreeze(getNamePath(parent, name));
    this.fullName = ArrayPrototypeJoin(this.namePath, ' ');
    this.benchId = JSONStringify([
      this.loc.file,
      this.namePath,
      this.params,
    ]);
    this.parentId = parent.suiteId;
    this.finished = false;
    this.result = null;
    this.completion = PromiseWithResolvers();
  }
}

class BenchContext {
  #closed = false;
  #done = false;
  #endCalled = false;
  #index;
  #invalid = false;
  #phase;
  #recordCalled = false;
  #sample = null;
  #startCalled = false;
  #startTime;

  constructor(bench, signal, phase, index) {
    this.name = bench.name;
    this.params = bench.params;
    this.signal = signal;
    this.#phase = phase;
    this.#index = index;
  }

  get index() {
    return this.#index;
  }

  get phase() {
    return this.#phase;
  }

  start() {
    if (this.#closed) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE('benchmark sample is no longer active');
    }
    if (this.#recordCalled) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE(
        'start() cannot be combined with record()');
    }
    if (this.#startCalled) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE(
        'start() must be called exactly once per benchmark sample');
    }
    this.#startCalled = true;
    this.#startTime = hrtime();
  }

  end(operations, options = kEmptyObject) {
    const endTime = hrtime();
    if (this.#closed) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE('benchmark sample is no longer active');
    }
    if (this.#recordCalled) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE(
        'end() cannot be combined with record()');
    }
    if (this.#endCalled) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE(
        'end() must be called exactly once per benchmark sample');
    }
    this.#endCalled = true;
    if (!this.#startCalled) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE('end() cannot be called before start()');
    }

    try {
      validateObject(options, 'options');
      const { detail } = options;
      validateInteger(operations, 'operations', 1, NumberMAX_SAFE_INTEGER);
      const duration = endTime - this.#startTime;
      if (duration === 0n) {
        throw new ERR_INVALID_STATE(
          'insufficient clock precision for benchmark sample');
      }
      this.#sample = {
        __proto__: null,
        operations,
        duration_ns: duration,
        rate: operations / (Number(duration) / 1e9),
      };
      if (detail !== undefined) {
        this.#sample.detail = structuredClone(detail);
      }
    } catch (error) {
      this.#invalid = true;
      throw error;
    }
    return cloneSample(this.#sample);
  }

  record(sample) {
    if (this.#closed) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE('benchmark sample is no longer active');
    }
    if (this.#recordCalled) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE(
        'record() must be called exactly once per benchmark sample');
    }
    if (this.#startCalled || this.#endCalled) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE(
        'record() cannot be combined with start() or end()');
    }
    this.#recordCalled = true;

    try {
      validateObject(sample, 'sample');
      const { detail, duration_ns, operations } = sample;
      validateInteger(
        operations, 'sample.operations', 1, NumberMAX_SAFE_INTEGER);
      if (typeof duration_ns !== 'bigint') {
        throw new ERR_INVALID_ARG_TYPE(
          'sample.duration_ns', 'bigint', duration_ns);
      }
      if (duration_ns <= 0n) {
        throw new ERR_OUT_OF_RANGE(
          'sample.duration_ns', 'a positive bigint', duration_ns);
      }
      if (duration_ns > 9_007_199_254_740_991n) {
        throw new ERR_OUT_OF_RANGE(
          'sample.duration_ns',
          'less than or equal to Number.MAX_SAFE_INTEGER',
          duration_ns);
      }
      this.#sample = {
        __proto__: null,
        operations,
        duration_ns,
        rate: operations / (Number(duration_ns) / 1e9),
      };
      if (detail !== undefined) {
        this.#sample.detail = structuredClone(detail);
      }
    } catch (error) {
      this.#invalid = true;
      throw error;
    }
    return cloneSample(this.#sample);
  }

  done() {
    if (this.#closed) {
      this.#invalid = true;
      throw new ERR_INVALID_STATE('benchmark sample is no longer active');
    }
    if (this.#phase !== 'measurement') {
      this.#invalid = true;
      throw new ERR_INVALID_STATE(
        'done() can only be called during a measured sample');
    }
    this.#done = true;
  }

  finish() {
    this.#closed = true;
    if (this.#invalid) {
      throw new ERR_INVALID_STATE(
        'benchmark sample violated the start()/end() contract');
    }
    if (!this.#recordCalled && !this.#startCalled) {
      throw new ERR_INVALID_STATE(
        'benchmark callback did not call start() or record()');
    }
    if (!this.#recordCalled && (!this.#endCalled || this.#sample === null)) {
      throw new ERR_INVALID_STATE(
        'benchmark callback did not call end()');
    }
    return {
      __proto__: null,
      done: this.#done,
      sample: this.#sample,
    };
  }

  close() {
    this.#closed = true;
  }
}

function arithmeticMean(values) {
  let sum = 0;
  let compensation = 0;
  for (let i = 0; i < values.length; i++) {
    const adjusted = values[i] - compensation;
    const next = sum + adjusted;
    compensation = (next - sum) - adjusted;
    sum = next;
  }
  return sum / values.length;
}

function summarizeSamples(samples) {
  const rates = [];
  let min = Infinity;
  let max = -Infinity;
  for (let i = 0; i < samples.length; i++) {
    const rate = samples[i].rate;
    ArrayPrototypePush(rates, rate);
    min = MathMin(min, rate);
    max = MathMax(max, rate);
  }

  const mean = arithmeticMean(rates);
  let variance = 0;
  for (let i = 0; i < rates.length; i++) {
    const difference = rates[i] - mean;
    variance += difference * difference;
  }
  variance /= rates.length;
  const stddev = MathSqrt(variance);

  const sorted = ArrayPrototypeSlice(rates);
  ArrayPrototypeSort(sorted, (a, b) => a - b);
  const middle = MathFloor(sorted.length / 2);
  const median = sorted.length % 2 === 0 ?
    (sorted[middle - 1] + sorted[middle]) / 2 : sorted[middle];

  const scale = MathMin(1_000_000, NumberMAX_SAFE_INTEGER / max);
  const histogram = createHistogram({ __proto__: null, figures: 5 });
  for (let i = 0; i < rates.length; i++) {
    const value = MathMax(
      1,
      MathMin(NumberMAX_SAFE_INTEGER, MathRound(rates[i] * scale)),
    );
    histogram.record(value);
  }

  const meanCI = histogram.meanCI();
  const histogramMean = meanCI.mean / scale;
  const medianCI = histogram.percentileCI(50);

  return {
    __proto__: null,
    mean,
    median,
    min,
    max,
    stddev,
    coefficientOfVariation: stddev / mean,
    confidenceInterval: {
      __proto__: null,
      lower: mean + meanCI.lower / scale - histogramMean,
      upper: mean + meanCI.upper / scale - histogramMean,
    },
    medianConfidenceInterval: {
      __proto__: null,
      lower: medianCI.lower / scale,
      upper: medianCI.upper / scale,
    },
    skewness: histogram.skewness,
  };
}

function normalizeArgs(type, name, options, fn) {
  if (typeof name === 'function') {
    fn = name;
    name = fn.name || '<anonymous>';
    options = kEmptyObject;
  } else if (name !== null && typeof name === 'object') {
    fn = options;
    options = name;
    name = fn?.name || '<anonymous>';
  } else if (typeof options === 'function') {
    fn = options;
    options = kEmptyObject;
  }

  validateFunction(fn, `${type} function`);
  validateString(name, `${type} name`);
  if (name.length === 0) {
    throw new ERR_INVALID_ARG_VALUE(`${type} name`, name, 'must not be empty');
  }
  validateObject(options, 'options');
  return { __proto__: null, fn, name, options };
}

module.exports = {
  Bench,
  BenchContext,
  Suite,
  normalizeArgs,
  summarizeSamples,
};
