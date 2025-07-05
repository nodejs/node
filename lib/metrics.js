/**
 * A metrics provider which reports to diagnostics_channel.
 *
 * # Metric Types
 *
 * - Counter: An increasing or decreasing value.
 * - Gauge: A snapshot of a single value in time.
 * - Timer: A duration in milliseconds.
 * - PullGauge: A gauge which updates its value by calling a function when sampled.
 * # TODO(qard):
 * - Histograms
 * - Distributions/Summaries
 */

'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  MathFloor,
  ObjectAssign,
  ObjectEntries,
  ObjectFreeze,
  ObjectKeys,
  SafeMap,
  SymbolDispose,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');

const {
  channel,
  hasChannel,
  subscribe,
  unsubscribe,
} = require('diagnostics_channel');
const { performance } = require('perf_hooks');

const newMetricChannel = channel('metrics:new');

/**
 * Represents a single reported metric.
 */
class MetricReport {
  #type;
  #name;
  #value;
  #meta;
  #time;

  /**
   * Constructs a new metric report.
   * @param {string} type The type of metric.
   * @param {string} name The name of the metric.
   * @param {number} value The value of the metric.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  constructor(type, name, value, meta) {
    this.#type = type;
    this.#name = name;
    this.#value = value;
    this.#meta = meta;
    this.#time = performance.now();
  }

  /**
   * The type of metric.
   * @property {string} type
   */
  get type() {
    return this.#type;
  }

  /**
   * The name of the metric.
   * @property {string} name
   */
  get name() {
    return this.#name;
  }

  /**
   * The value of the metric.
   * @property {number} value
   */
  get value() {
    return this.#value;
  }

  /**
   * Additional metadata to include with the report.
   * @property {object} meta
   */
  get meta() {
    return this.#meta;
  }

  /**
   * The timestamp of the report.
   * @property {number} time
   */
  get time() {
    return this.#time;
  }
}

/**
 * Represents a metric which can be reported to.
 */
class Metric {
  #channel;
  #type;
  #name;
  #meta;

  /**
   * Constructs a new metric.
   * @param {string} type The type of metric.
   * @param {string} name The name of the metric.
   * @param {object} [meta] Additional metadata to include with the metric.
   */
  constructor(type, name, meta) {
    if (!metricTypeNames.includes(type)) {
      throw new ERR_INVALID_ARG_VALUE('type', type, wrongTypeErr);
    }
    if (typeof name !== 'string' || !name) {
      throw new ERR_INVALID_ARG_TYPE('name', ['string'], name);
    }
    if (meta !== undefined && typeof meta !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('meta', ['object', 'undefined'], meta);
    }

    this.#type = type;
    this.#name = name;
    this.#meta = meta;

    // Before acquiring the channel, check if it already exists.
    const exists = hasChannel(this.channelName);
    this.#channel = channel(this.channelName);

    // If the channel is new and there are new channel subscribers,
    // publish the metric to the new metric channel.
    if (!exists && newMetricChannel.hasSubscribers) {
      newMetricChannel.publish(this);
    }
  }

  /**
   * The type of metric.
   * @property {string} type
   */
  get type() {
    return this.#type;
  }

  /**
   * The name of the metric.
   * @property {string} name
   */
  get name() {
    return this.#name;
  }

  /**
   * Additional metadata to include with the metric.
   * @property {object} meta
   */
  get meta() {
    return this.#meta;
  }

  /**
   * The channel name of the metric.
   * @property {string} channelName
   */
  get channelName() {
    return `metrics:${this.type}:${this.name}`;
  }

  /**
   * The channel for this metric.
   * @property {Channel} channel
   */
  get channel() {
    return this.#channel;
  }

  /**
   * Whether the metric should report values. If there are no subscribers,
   * metric preparation and report construction can be skipped.
   * @property {boolean} shouldReport
   */
  get shouldReport() {
    return this.#channel.hasSubscribers;
  }

  /**
   * Report a value to the metric.
   * @param {number} value The value to report.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  report(value, meta) {
    // Skip report construction if there are no subscribers.
    if (!this.shouldReport) return;
    const report = new MetricReport(this.type, this.name, value,
                                    ObjectAssign({}, this.meta, meta));
    this.#channel.publish(report);
  }
}

/**
 * Represents a snapshot of a value in time. Will report the value every time
 * reset() is called, or when applyDelta() is called with a non-zero value.
 */
class Gauge {
  /**
   * The metric to report to.
   * @property {Metric} metric
   */

  /**
   * The value of the gauge.
   * @property {number} value
   */

  /**
   * @param {Metric} metric The metric to report to.
   */
  constructor(metric) {
    if (!(metric instanceof Metric)) {
      throw new ERR_INVALID_ARG_TYPE('metric', ['Metric'], metric);
    }
    this.metric = metric;
    this.value = 0;
  }

  /**
   * Set the gauge value.
   * @param {number} value The value to set the gauge to.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  reset(value = 0, meta) {
    this.value = value;
    this.metric.report(value, meta);
  }

  /**
   * Apply a delta to the gauge.
   * @param {number} value The delta to apply to the gauge.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  applyDelta(value, meta) {
    this.reset(this.value + value, meta);
  }
}

/**
 * An increasing or decreasing value.
 */
class Counter extends Gauge {
  /**
   * Increment the counter. Negative values invert to positive.
   * @param {number} [n] The amount to increment the counter by. Defaults to 1.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  increment(n = 1, meta) {
    if (!this.metric.shouldReport) return;

    if (typeof n === 'object') {
      meta = n;
      n = 1;
    }

    this.applyDelta(n, meta);
  }

  /**
   * Decrement the counter. Negative values invert to positive.
   * @param {number} [n] The amount to decrement the counter by. Defaults to 1.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  decrement(n = 1, meta) {
    if (!this.metric.shouldReport) return;

    if (typeof n === 'object') {
      meta = n;
      n = 1;
    }

    this.applyDelta(-n, meta);
  }
}

/**
 * A floating point number which represents a length of time in milliseconds.
 */
class Timer extends Gauge {
  #meta;

  /**
   * The start time of the timer.
   * @property {number} start
   */

  /**
   * End time of timer. If undefined, timer is still running.
   * @property {number|undefined} end
   */

  /**
   * Duration of timer in milliseconds. If undefined, timer is still running.
   * @property {number|undefined} duration
   */

  /**
   * Construct a new timer.
   * @param {Metric} metric The metric to report to.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  constructor(metric, meta) {
    super(metric);
    if (meta !== undefined && typeof meta !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('meta', ['object', 'undefined'], meta);
    }
    this.#meta = meta;

    this.start = performance.now();
    this.end = undefined;
    this.duration = undefined;
  }

  /**
   * Additional metadata to include with the report.
   * @property {object} meta
   */
  get meta() {
    return ObjectAssign({}, this.metric.meta, this.#meta);
  }

  /**
   * Stop the timer and report the duration.
   * @param {object} [meta] Additional metadata to include with the report.
   * @returns {number} The duration in milliseconds.
   */
  stop(meta) {
    if (this.end !== undefined) return;
    if (!this.metric.shouldReport) return;
    this.end = performance.now();
    this.duration = this.end - this.start;
    this.reset(this.duration, ObjectAssign({}, this.#meta, meta));
    return this.duration;
  }

  /**
   * Support `using` syntax to automatically stop the timer when done.
   */
  [SymbolDispose]() {
    this.stop();
  }
}

/**
 * A gauge which updates its value by calling a function when sampled.
 */
class PullGauge extends Gauge {
  #fn;

  /**
   * Construct a new pull gauge.
   * @param {Metric} metric The metric to report to.
   * @param {Function} fn The function to call to get the gauge value.
   */
  constructor(metric, fn) {
    super(metric);

    if (typeof fn !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('fn', ['function'], fn);
    }

    this.#fn = fn;
  }

  /**
   * Sample the gauge by calling the function and reporting the value.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  sample(meta) {
    const value = this.#fn();
    this.reset(value, meta);
    return value;
  }
}

/**
 * A factory for creating Timers for the given metric.
 */
class TimerFactory {
  /**
   * The metric to report to.
   * @property {Metric} metric
   */

  /**
   * Construct a new Timer factory.
   * @param {Metric} metric The metric to report to.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  constructor(metric) {
    if (!(metric instanceof Metric)) {
      throw new ERR_INVALID_ARG_TYPE('metric', ['Metric'], metric);
    }
    this.metric = metric;
    ObjectFreeze(this);
  }

  /**
   * Create a new timer with the given metadata.
   * @param {object} [meta] Additional metadata to include with this timer.
   * @returns {Timer} A new Timer instance with the combined metadata.
   */
  create(meta) {
    return new Timer(this.metric, meta);
  }
}

/**
 * Create a timer metric.
 * @param {string} name The name of the timer.
 * @param {object} [meta] Additional metadata to include with the report.
 * @returns {object} An object with a create method to create new timers.
 */
function timer(name, meta) {
  const metric = new Metric('timer', name, meta);
  return new TimerFactory(metric);
}

// Map of metric types to their constructors.
const metricTypes = {
  counter: Counter,
  gauge: Gauge,
  pullGauge: PullGauge,
  timer: Timer,
};

const metricTypeNames = ObjectKeys(metricTypes);
const wrongTypeErr = `must be one of: ${metricTypeNames.join(', ')}`;

/**
 * Create a function to directly create a metric of a specific type.
 * @param {string} type The type of metric to create.
 * @returns {Function} A function which creates a metric of the specified type.
 * @private
 */
function direct(type) {
  if (!metricTypeNames.includes(type)) {
    throw new ERR_INVALID_ARG_VALUE('type', type, wrongTypeErr);
  }
  const Type = metricTypes[type];

  return function makeMetricType(name, ...args) {
    let meta;
    if (typeof args[args.length - 1] === 'object') {
      meta = args.pop();
    }

    const metric = new Metric(type, name, meta);
    return new Type(metric, ...args);
  };
}

/**
 * Create a counter metric.
 * @param {string} name The name of the counter.
 * @param {object} [meta] Additional metadata to include with the report.
 * @returns {Counter} The counter metric.
 */
const counter = direct('counter');

/**
 * Create a gauge metric.
 * @param {string} name The name of the gauge.
 * @param {object} [meta] Additional metadata to include with the report.
 * @returns {Gauge} The gauge metric.
 */
const gauge = direct('gauge');

/**
 * Create a raw metric.
 * @param {string} type The type of metric to create (e.g., 'gauge', 'counter').
 * @param {string} name The name of the metric.
 * @param {object} [meta] Additional metadata to include with the report.
 * @returns {Metric} The raw metric.
 */
function metric(type, name, meta) {
  return new Metric(type, name, meta);
}

/**
 * Create a pull gauge metric.
 * @param {string} name The name of the pull gauge.
 * @param {Function} fn The function to call to get the gauge value.
 * @param {object} [meta] Additional metadata to include with the report.
 * @returns {PullGauge} The pull gauge metric.
 */
const pullGauge = direct('pullGauge');

module.exports = {
  MetricReport,
  Metric,
  Gauge,
  Counter,
  Timer,
  PullGauge,
  TimerFactory,


  counter,
  gauge,
  metric,
  pullGauge,
  timer,
};
