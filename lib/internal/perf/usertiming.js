'use strict';

const {
  ObjectKeys,
  SafeMap,
  SafeSet,
  SafeArrayIterator,
} = primordials;

const { InternalPerformanceEntry } = require('internal/perf/performance_entry');
const { now } = require('internal/perf/utils');
const { enqueue } = require('internal/perf/observe');
const nodeTiming = require('internal/perf/nodetiming');

const {
  validateNumber,
  validateObject,
  validateString,
} = require('internal/validators');

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_PERFORMANCE_MARK,
    ERR_PERFORMANCE_INVALID_TIMESTAMP,
    ERR_PERFORMANCE_MEASURE_INVALID_OPTIONS,
  },
} = require('internal/errors');

const marks = new SafeMap();

const nodeTimingReadOnlyAttributes = new SafeSet(new SafeArrayIterator([
  'nodeStart',
  'v8Start',
  'environment',
  'loopStart',
  'loopExit',
  'bootstrapComplete',
]));

function getMark(name) {
  if (name === undefined) return;
  if (typeof name === 'number') {
    if (name < 0)
      throw new ERR_PERFORMANCE_INVALID_TIMESTAMP(name);
    return name;
  }
  name = `${name}`;
  if (nodeTimingReadOnlyAttributes.has(name))
    return nodeTiming[name];
  const ts = marks.get(name);
  if (ts === undefined)
    throw new ERR_INVALID_PERFORMANCE_MARK(name);
  return ts;
}

class PerformanceMark extends InternalPerformanceEntry {
  constructor(name, options = {}) {
    name = `${name}`;
    if (nodeTimingReadOnlyAttributes.has(name))
      throw new ERR_INVALID_ARG_VALUE('name', name);
    validateObject(options, 'options');
    const {
      detail,
      startTime = now(),
    } = options;
    validateNumber(startTime, 'startTime');
    if (startTime < 0)
      throw new ERR_PERFORMANCE_INVALID_TIMESTAMP(startTime);
    marks.set(name, startTime);
    super(name, 'mark', startTime, 0, detail);
    enqueue(this);
  }
}

class PerformanceMeasure extends InternalPerformanceEntry {
  constructor(name, start, duration, detail) {
    super(name, 'measure', start, duration, detail);
    enqueue(this);
  }
}

function mark(name, options = {}) {
  return new PerformanceMark(name, options);
}

function calculateStartDuration(startOrMeasureOptions, endMark) {
  startOrMeasureOptions ??= 0;
  let detail;
  let start;
  let end;
  let duration;
  if (typeof startOrMeasureOptions === 'object' &&
      ObjectKeys(startOrMeasureOptions).length) {
    ({
      start,
      end,
      duration,
      detail,
    } = startOrMeasureOptions);
    if (endMark !== undefined) {
      throw new ERR_PERFORMANCE_MEASURE_INVALID_OPTIONS(
        'endMark must not be specified');
    }
    if (start === undefined && end === undefined) {
      throw new ERR_PERFORMANCE_MEASURE_INVALID_OPTIONS(
        'One of options.start or options.end is required');
    }
    if (start !== undefined && end !== undefined && duration !== undefined) {
      throw new ERR_PERFORMANCE_MEASURE_INVALID_OPTIONS(
        'Must not have options.start, options.end, and ' +
        'options.duration specified');
    }
    start = getMark(start);
    duration = getMark(duration);
  } else {
    start = getMark(startOrMeasureOptions);
  }

  end = getMark(endMark || end) ??
    ((start !== undefined && duration !== undefined) ?
      start + duration : now());

  start ??= (duration !== undefined) ? end - duration : 0;

  duration ??= end - start;

  return { start, duration, detail };
}

function measure(name, startOrMeasureOptions, endMark) {
  validateString(name, 'name');
  const {
    start,
    duration,
    detail
  } = calculateStartDuration(startOrMeasureOptions, endMark);
  return new PerformanceMeasure(name, start, duration, detail);
}

function clearMarks(name) {
  if (name !== undefined) {
    name = `${name}`;
    if (nodeTimingReadOnlyAttributes.has(name))
      throw new ERR_INVALID_ARG_VALUE('name', name);
    marks.delete(name);
    return;
  }
  marks.clear();
}

module.exports = {
  PerformanceMark,
  clearMarks,
  mark,
  measure,
};
