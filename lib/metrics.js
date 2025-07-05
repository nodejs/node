/**
 * A metrics provider which reports to diagnostics_channel.
 *
 * # Metric Types
 *
 * - Counter: An increasing or decreasing value.
 * - Gauge: A snapshot of a single value in time.
 * - Timer: A duration in milliseconds.
 * - PeriodicGauge: A gauge which periodically updates its value by calling a function.
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
const { setInterval, clearInterval } = require('internal/timers');

const {
  channel,
  hasChannel,
  subscribe,
  unsubscribe,
} = require('diagnostics_channel');
const { performance } = require('perf_hooks');

const newMetricChannel = channel('metrics:new');

/**
 * Mix two metadata objects together.
 * @param {object} a The first metadata object.
 * @param {object} b The second metadata object.
 * @returns {object} The mixed metadata.
 * @private
 */
function mixMeta(a, b) {
  if (a === undefined) return b;
  if (b === undefined) return a;
  return ObjectAssign({}, a, b);
}

/**
 * Represents a single reported metric.
 */
class MetricReport {
  /**
   * The type of metric.
   * @property {string} type
   */

  /**
   * The name of the metric.
   * @property {string} name
   */

  /**
   * The value of the metric.
   * @property {number} value
   */

  /**
   * Additional metadata to include with the report.
   * @property {object} meta
   */

  /**
   * Constructs a new metric report.
   * @param {string} type The type of metric.
   * @param {string} name The name of the metric.
   * @param {number} value The value of the metric.
   * @param {object} [meta] Additional metadata to include with the report.
   */
  constructor(type, name, value, meta) {
    this.type = type;
    this.name = name;
    this.value = value;
    this.meta = meta;
    this.time = performance.now();
    ObjectFreeze(this);
  }

  /**
   * Convert the metric report to a statsd-compatible string.
   * @returns {string} The statsd-formatted metric report.
   */
  toStatsd() {
    const { type, name, value } = this;
    return `${name}:${value}|${this.#statsdType(type)}`;
  }

  /*
   * Convert the metric type to a statsd type.
   *
   * @param {string} type The metric type.
   * @returns {string} The statsd type.
   * @private
   */
  #statsdType(type) {
    return {
      counter: 'c',
      gauge: 'g',
      periodicGauge: 'g',
      timer: 'ms',
    }[type];
  }

  /**
   * Convert the metric report to a Dogstatsd-compatible string.
   * @returns {string} The Dogstatsd-formatted metric report.
   */
  toDogStatsd() {
    return `${this.toStatsd()}${this.#dogstatsdTags()}`;
  }

  /*
   * Pack metadata into Dogstatsd-compatible tags.
   *
   * @returns {string} The packed metadata.
   * @private
   */
  #dogstatsdTags() {
    const entries = ObjectEntries(this.meta);
    const pairs = ArrayPrototypeMap(entries, ({ 0: k, 1: v }) => `${k}:${v}`);
    const tags = ArrayPrototypeJoin(pairs, ',');
    return tags.length ? `|${tags}` : '';
  }

  /**
   * Convert the metric report to a graphite-compatible string.
   * @returns {string} The graphite-formatted metric report.
   */
  toGraphite() {
    const { name, value, time } = this;
    return `${name} ${value} ${MathFloor(time / 1000)}`;
  }

  /**
   * Convert the metric report to a Prometheus-compatible string.
   * @returns {string} The Prometheus-formatted metric report.
   */
  toPrometheus() {
    const { name, value, time } = this;
    return `${name}${this.#prometheusLabels()} ${value} ${time}`;
  }

  /*
   * Pack metadata into Prometheus-compatible labels.
   *
   * @returns {string} The packed metadata.
   * @private
   */
  #prometheusLabels() {
    const entries = ObjectEntries(this.meta);
    const pairs = ArrayPrototypeMap(entries, ({ 0: k, 1: v }) => `${k}="${v}"`);
    const labels = ArrayPrototypeJoin(pairs, ',');
    return labels.length ? `{${labels}}` : '';
  }
}

/**
 * Represents a metric which can be reported to.
 */
class Metric {
  #channel;

  /**
   * The type of metric.
   * @property {string} type
   */

  /**
   * The name of the metric.
   * @property {string} name
   */

  /**
   * Additional metadata to include with the metric.
   * @property {object} meta
   */

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

    this.type = type;
    this.name = name;
    this.meta = meta;

    // Before acquiring the channel, check if it already exists.
    const exists = hasChannel(this.channelName);
    this.#channel = channel(this.channelName);

    // If the channel is new and there are new channel subscribers,
    // publish the metric to the new metric channel.
    if (!exists && newMetricChannel.hasSubscribers) {
      newMetricChannel.publish(this);
    }

    ObjectFreeze(this);
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
                                    mixMeta(this.meta, meta));
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
    return mixMeta(this.metric.meta, this.#meta);
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
    this.reset(this.duration, mixMeta(this.#meta, meta));
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
 * A gauge which periodically updates its value by calling a function and
 * setting the value to the result.
 */
class PeriodicGauge extends Gauge {
  #timer;
  #interval;
  #fn;

  /**
   * Construct a new periodic gauge.
   * @param {Metric} metric The metric to report to.
   * @param {number} interval The interval in milliseconds to update the gauge.
   * @param {Function} fn The function to call to update the gauge.
   */
  constructor(metric, interval, fn) {
    super(metric);

    if (typeof interval !== 'number' || interval <= 0) {
      throw new ERR_INVALID_ARG_TYPE('interval', ['number'], interval);
    }
    if (typeof fn !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('fn', ['function'], fn);
    }

    this.#timer = undefined;
    this.#interval = interval;
    this.#fn = fn;

    this.schedule();
  }

  /**
   * Schedule the update timer.
   */
  schedule() {
    this.stop();

    this.#timer = setInterval(() => {
      this.reset(this.#fn());
    }, this.interval);

    // Don't keep the process alive just for this timer.
    this.#timer.unref();
  }

  /**
   * The interval in milliseconds at which to update the value. If changed,
   * the timer will be rescheduled.
   * @property {number} interval
   */
  set interval(interval) {
    if (typeof interval !== 'number' || interval <= 0) {
      throw new ERR_INVALID_ARG_TYPE('interval', ['number'], interval);
    }
    this.#interval = interval;
    this.schedule();
  }
  get interval() {
    return this.#interval;
  }

  /**
   * Stop the periodic gauge.
   */
  stop() {
    if (this.#timer !== undefined) {
      clearInterval(this.#timer);
      this.#timer = undefined;
    }
  }

  /**
   * Reference the timer to prevent to loop from exiting.
   */
  ref() {
    this.#timer?.ref();
  }

  /**
   * Unreference the timer to allow the loop to exit.
   */
  unref() {
    this.#timer?.unref();
  }

  /**
   * Support `using` syntax to automatically stop the periodic gauge when done.
   */
  [SymbolDispose]() {
    this.stop();
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
  periodicGauge: PeriodicGauge,
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
 * Create a periodic gauge metric.
 * @param {string} name The name of the periodic gauge.
 * @param {number} interval The interval in milliseconds to update the gauge.
 * @param {Function} fn The function to call to update the gauge.
 * @param {object} [meta] Additional metadata to include with the report.
 * @returns {PeriodicGauge} The periodic gauge metric.
 */
const periodicGauge = direct('periodicGauge');

module.exports = {
  MetricReport,
  Metric,
  Gauge,
  Counter,
  Timer,
  PeriodicGauge,
  TimerFactory,


  counter,
  gauge,
  metric,
  periodicGauge,
  timer,
};
