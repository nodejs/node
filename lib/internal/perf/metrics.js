'use strict';

/**
 * This provides a flexible metrics core based on diagnostics_channel design
 * principles. It is meant to do the minimum work required at each stage to
 * enable maximal flexibility of the next.
 *
 * It follows a three-layer architecture:
 * 1. Value Producers - Generates singular values (direct, observable, timer)
 * 2. Descriptors - Metric values are grouped through a descriptor
 * 3. Consumers - Multi-tenant continuous aggregation of selected descriptors
 */

const {
  Array,
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  BigInt,
  MathRandom,
  MathRound,
  Number,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectKeys,
  SafeMap,
  SafeSet,
  Symbol,
  SymbolDispose,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const {
  kEmptyObject,
} = require('internal/util');

const {
  validateFunction,
  validateNumber,
  validateObject,
  validateString,
} = require('internal/validators');

const { performance } = require('internal/perf/performance');
const { createHistogram } = require('internal/histogram');
const {
  setInterval,
  clearInterval,
} = require('timers');
const { triggerUncaughtException } = internalBinding('errors');
const { ObjectIdentity } = require('internal/util/object_identity');

// Private symbols for module-internal methods
const kReset = Symbol('kReset');
const kAddSubscriber = Symbol('kAddSubscriber');
const kRemoveSubscriber = Symbol('kRemoveSubscriber');
const kEnableTimestamp = Symbol('kEnableTimestamp');
const kDisableTimestamp = Symbol('kDisableTimestamp');
const kAdd = Symbol('kAdd');
const kRemove = Symbol('kRemove');

// =============================================================================
// Attribute Identity for Performance
// =============================================================================

/**
 * Shared ObjectIdentity instance for attribute hashing.
 * Uses xxHash32 algorithm (industry standard, same as OpenTelemetry/Prometheus).
 * Returns numeric hashes for efficient Map lookups.
 */
const attributeIdentity = new ObjectIdentity({
  sortedKeysCacheSize: 1000,
});

// =============================================================================
// Layer 2: Data Representation
// =============================================================================

/**
 * InstrumentationScope identifies the library/module producing metrics.
 * Immutable once created.
 * See: https://opentelemetry.io/docs/specs/otel/glossary/#instrumentation-scope
 */
class InstrumentationScope {
  #name;
  #version;
  #schemaUrl;
  #json;

  constructor(name, version, schemaUrl) {
    validateString(name, 'name');
    if (version !== undefined) validateString(version, 'version');
    if (schemaUrl !== undefined) validateString(schemaUrl, 'schemaUrl');
    this.#name = name;
    this.#version = version;
    this.#schemaUrl = schemaUrl;
  }

  get name() {
    return this.#name;
  }

  get version() {
    return this.#version;
  }

  get schemaUrl() {
    return this.#schemaUrl;
  }

  toJSON() {
    this.#json ??= {
      name: this.#name,
      version: this.#version,
      schemaUrl: this.#schemaUrl,
    };
    return this.#json;
  }
}

/**
 * Exemplar represents a sample measurement with trace context.
 * Used to correlate metrics with distributed traces.
 * See: https://opentelemetry.io/docs/specs/otel/metrics/data-model/#exemplars
 */
class Exemplar {
  #value;
  #timestamp;
  #traceId;
  #spanId;
  #filteredAttributes;

  constructor(value, timestamp, traceId, spanId, filteredAttributes = kEmptyObject) {
    this.#value = value;
    this.#timestamp = timestamp;
    this.#traceId = traceId;
    this.#spanId = spanId;
    this.#filteredAttributes = filteredAttributes;
  }

  get value() {
    return this.#value;
  }

  get timestamp() {
    return this.#timestamp;
  }

  get traceId() {
    return this.#traceId;
  }

  get spanId() {
    return this.#spanId;
  }

  get filteredAttributes() {
    return this.#filteredAttributes;
  }

  toJSON() {
    return {
      value: this.#value,
      timestamp: this.#timestamp,
      traceId: this.#traceId,
      spanId: this.#spanId,
      filteredAttributes: this.#filteredAttributes,
    };
  }
}

/**
 * Immutable descriptor for a metric. Constructed once, reused for all values.
 * Consumers can use reference equality (===) for fast comparisons.
 */
class MetricDescriptor {
  #name;
  #unit;
  #description;
  #scope; // InstrumentationScope
  #channel; // Lazily created diagnostics_channel for this metric
  #json; // Cached JSON representation

  constructor(name, unit, description, scope) {
    this.#name = name;
    this.#unit = unit;
    this.#description = description;
    this.#scope = scope;
  }

  get name() {
    return this.#name;
  }

  get unit() {
    return this.#unit;
  }

  get description() {
    return this.#description;
  }

  get scope() {
    return this.#scope;
  }

  /**
   * Get the diagnostics_channel for this metric (lazily created).
   * Used by DiagnosticsChannelConsumer for efficient publishing.
   */
  get channel() {
    if (!this.#channel) {
      const dc = require('diagnostics_channel');
      this.#channel = dc.channel(`metrics:${this.#name}`);
    }
    return this.#channel;
  }

  toJSON() {
    this.#json ??= {
      name: this.#name,
      unit: this.#unit,
      description: this.#description,
      scope: this.#scope?.toJSON(),
    };
    return this.#json;
  }
}

// =============================================================================
// Layer 1: Value Producers
// =============================================================================

// Timer pool for object reuse to reduce GC pressure
const timerPool = [];
const kMaxPoolSize = 100;

/**
 * A timer that measures duration and records it to a metric when stopped.
 */
class Timer {
  #metric;
  #attributes;
  #startTime;
  #stopped;

  /**
   * @param {Metric} metric - The metric to record duration to
   * @param {object} [attributes] - Attributes to include with the recorded value
   */
  constructor(metric, attributes = kEmptyObject) {
    validateObject(attributes, 'attributes');
    this.#metric = metric;
    this.#attributes = attributes;
    this.#startTime = performance.now();
    this.#stopped = false;
  }

  get startTime() {
    return this.#startTime;
  }

  /**
   * Stop the timer and record the duration to the metric.
   * @returns {number} The duration in milliseconds
   * @throws {ERR_INVALID_STATE} If timer is already stopped
   */
  stop() {
    if (this.#stopped) {
      throw new ERR_INVALID_STATE('Timer has already been stopped');
    }
    this.#stopped = true;
    const endTime = performance.now();
    const duration = endTime - this.#startTime;
    // Pass endTime as timestamp to avoid redundant now() call in record()
    this.#metric.record(duration, this.#attributes, endTime);
    // Return to pool for reuse
    this.#returnToPool();
    return duration;
  }

  /**
   * Support `using` syntax to automatically stop the timer.
   * Does not throw if already stopped.
   */
  [SymbolDispose]() {
    if (!this.#stopped) {
      this.stop();
    }
  }

  /**
   * Reset the timer for reuse (internal only).
   * @param {Metric} metric - The metric to record duration to
   * @param {object} attributes - Attributes to include with the recorded value
   */
  [kReset](metric, attributes) {
    this.#metric = metric;
    this.#attributes = attributes;
    this.#startTime = performance.now();
    this.#stopped = false;
  }

  /**
   * Return timer to pool if space available (internal only).
   */
  #returnToPool() {
    if (timerPool.length < kMaxPoolSize) {
      // Clear references to allow GC of metric/attributes
      this.#metric = null;
      this.#attributes = null;
      ArrayPrototypePush(timerPool, this);
    }
  }
}

// =============================================================================
// Layer 2: Data Representation
// =============================================================================

/**
 * A metric that records values. Values are immediately dispatched to subscribers.
 *
 * Each metric maintains its own subscriber list (like diagnostics_channel).
 * This eliminates per-value identity lookups - subscribers are registered once
 * and called directly on each value.
 */
class Metric {
  #descriptor;
  #observable;
  #subscribers = []; // Direct subscriber list (like dc channel)
  #makeTimestamp = (ts) => ts; // Default: pass through unchanged
  #timestampConsumers = 0; // Reference count for timestamp-needing consumers
  #closed = false;

  /**
   * @param {string} name - The metric name
   * @param {object} [options]
   * @param {string} [options.unit] - The unit of measurement
   * @param {string} [options.description] - Human-readable description
   * @param {Function} [options.observable] - Callback for observable metrics
   * @param {InstrumentationScope} [options.scope] - The instrumentation scope
   */
  constructor(name, options = kEmptyObject) {
    validateString(name, 'name');
    if (name === '') {
      throw new ERR_INVALID_ARG_VALUE('name', name, 'must not be empty');
    }
    validateObject(options, 'options');

    const { unit, description, observable, scope } = options;
    if (unit !== undefined) validateString(unit, 'options.unit');
    if (description !== undefined) validateString(description, 'options.description');
    if (observable !== undefined) validateFunction(observable, 'options.observable');
    if (scope !== undefined && !(scope instanceof InstrumentationScope)) {
      throw new ERR_INVALID_ARG_TYPE('options.scope', 'InstrumentationScope', scope);
    }

    this.#descriptor = new MetricDescriptor(name, unit, description, scope);
    this.#observable = observable;

    // Add to global registry, notifying consumers
    registry[kAdd](this);
  }

  get descriptor() {
    return this.#descriptor;
  }

  get isObservable() {
    return this.#observable !== undefined;
  }

  /**
   * Record a value. Immediately dispatches to all subscribers.
   * @param {number|bigint} value - The value to record
   * @param {object} [attributes] - Additional attributes for this value
   * @param {number} [timestamp] - Optional timestamp (defaults to now)
   */
  record(value, attributes = kEmptyObject, timestamp) {
    if (typeof value !== 'number' && typeof value !== 'bigint') {
      throw new ERR_INVALID_ARG_TYPE('value', ['number', 'bigint'], value);
    }
    validateObject(attributes, 'attributes');

    const subscribers = this.#subscribers;
    const subCount = subscribers.length;
    if (subCount === 0) return;

    timestamp = this.#makeTimestamp(timestamp);

    // Fast iteration - no lookups, only interested parties
    for (let i = 0; i < subCount; i++) {
      subscribers[i].onValue(value, timestamp, attributes);
    }
  }

  /**
   * Sample an observable metric. Called during collect().
   * Only dispatches to the specified subscriber to maintain consumer isolation.
   * Observable callbacks receive a facade with `record()` and `descriptor`:
   *
   * ```js
   * observable: (metric) => {
   *   metric.record(10, { cpu: 0 });
   *   metric.record(20, { cpu: 1 });
   * }
   * ```
   *
   * Error handling: Observable callback errors are non-fatal. They emit a warning
   * and continue with other observables. This follows the principle that a single
   * misbehaving observable should not break the entire metrics collection.
   * @param {MetricSubscriber} subscriber - The specific subscriber to receive values
   * @returns {boolean} True if any values were sampled
   */
  sample(subscriber) {
    if (!this.#observable || this.#closed) return false;

    const timestamp = this.#makeTimestamp(undefined);
    let hasValues = false;
    const descriptor = this.#descriptor;

    // Facade passed to callback for multi-value reporting with per-consumer isolation
    const facade = {
      record(value, attributes = kEmptyObject) {
        if (typeof value !== 'number' && typeof value !== 'bigint') {
          throw new ERR_INVALID_ARG_TYPE('value', ['number', 'bigint'], value);
        }
        validateObject(attributes, 'attributes');
        hasValues = true;
        subscriber.onValue(value, timestamp, attributes);
      },
      get descriptor() {
        return descriptor;
      },
    };

    try {
      this.#observable(facade);
    } catch (err) {
      // Observable errors are non-fatal to the collection loop, but should
      // still be surfaced via uncaughtException (like diagnostics_channel)
      process.nextTick(() => {
        triggerUncaughtException(err, false);
      });
      return hasValues;
    }

    return hasValues;
  }

  /**
   * Create a timer that records its duration to this metric when stopped.
   * @param {object} [attributes] - Attributes to include with the recorded value
   * @returns {Timer}
   */
  startTimer(attributes = kEmptyObject) {
    validateObject(attributes, 'attributes');
    // Try to reuse a pooled timer
    if (timerPool.length > 0) {
      const timer = timerPool.pop();
      timer[kReset](this, attributes);
      return timer;
    }
    return new Timer(this, attributes);
  }

  /**
   * Add a subscriber to this metric (internal).
   * @param {MetricSubscriber} subscriber - The subscriber to add
   * @returns {Function} Unsubscribe function
   */
  [kAddSubscriber](subscriber) {
    ArrayPrototypePush(this.#subscribers, subscriber);
    return () => this[kRemoveSubscriber](subscriber);
  }

  /**
   * Remove a subscriber from this metric (internal).
   * @param {MetricSubscriber} subscriber - The subscriber to remove
   */
  [kRemoveSubscriber](subscriber) {
    const idx = ArrayPrototypeIndexOf(this.#subscribers, subscriber);
    if (idx !== -1) {
      ArrayPrototypeSplice(this.#subscribers, idx, 1);
    }
  }

  /**
   * Enable timestamp generation for this metric (internal).
   * Called by consumers that need timestamps (lastValue, DC consumer).
   * Uses reference counting to track how many consumers need timestamps.
   */
  [kEnableTimestamp]() {
    this.#timestampConsumers++;
    if (this.#timestampConsumers === 1) {
      this.#makeTimestamp = (ts) => ts ?? performance.now();
    }
  }

  /**
   * Disable timestamp generation for this metric (internal).
   * Called when a consumer that needed timestamps is closed.
   * When no consumers need timestamps, reverts to pass-through.
   */
  [kDisableTimestamp]() {
    this.#timestampConsumers--;
    if (this.#timestampConsumers === 0) {
      this.#makeTimestamp = (ts) => ts;
    }
  }

  /**
   * Check if this metric is closed.
   * @returns {boolean}
   */
  get isClosed() {
    return this.#closed;
  }

  /**
   * Unregister this metric and notify all consumers.
   * Use for lifecycle management when a metric is no longer needed.
   * Consumers will be notified via onMetricClosed() so they can clean up.
   */
  close() {
    if (this.#closed) return;
    this.#closed = true;

    // Notify registry (which notifies consumers)
    registry[kRemove](this);

    // Clear subscribers
    this.#subscribers = [];
    this.#timestampConsumers = 0;
  }
}

// =============================================================================
// Layer 3: Consumer Infrastructure
// =============================================================================

const kAggregatorSum = 'sum';
const kAggregatorLastValue = 'lastValue';
const kAggregatorHistogram = 'histogram';
const kAggregatorSummary = 'summary';

/**
 * Built-in aggregators
 */
const builtinAggregators = {
  [kAggregatorSum]: {
    createState(config) {
      return {
        sum: 0,
        count: 0,
        monotonic: config.monotonic ?? false,
      };
    },
    aggregate(state, value) {
      if (state.monotonic && value < 0) {
        // Ignore negative values for monotonic sums
        return;
      }
      if (typeof value === 'bigint') {
        // Convert sum to bigint if needed
        state.sum = BigInt(state.sum) + value;
      } else if (typeof state.sum === 'bigint') {
        // Sum is already bigint, convert value
        state.sum += BigInt(value);
      } else {
        state.sum += value;
      }
      state.count++;
    },
    finalize(state) {
      return {
        sum: state.sum,
        count: state.count,
      };
    },
    resetState(state) {
      state.sum = 0;
      state.count = 0;
    },
  },

  [kAggregatorLastValue]: {
    needsTimestamp: true,
    createState() {
      return {
        value: undefined,
        timestamp: undefined,
      };
    },
    aggregate(state, value, timestamp) {
      state.value = value;
      state.timestamp = timestamp;
    },
    finalize(state) {
      return {
        value: state.value,
        timestamp: state.timestamp,
      };
    },
    resetState(state) {
      state.value = undefined;
      state.timestamp = undefined;
    },
  },

  [kAggregatorHistogram]: {
    createState(config) {
      const boundaries = config.boundaries ?? [10, 50, 100, 500, 1000];
      const buckets = new Array(boundaries.length + 1);
      for (let i = 0; i < buckets.length; i++) {
        buckets[i] = 0;
      }
      return {
        boundaries,
        buckets,
        sum: 0,
        count: 0,
        min: Infinity,
        max: -Infinity,
      };
    },
    aggregate(state, value) {
      const numValue = typeof value === 'bigint' ? Number(value) : value;
      state.sum += numValue;
      state.count++;
      if (numValue < state.min) state.min = numValue;
      if (numValue > state.max) state.max = numValue;

      // Find bucket using binary search for O(log n) lookup
      // Values <= boundary go in that bucket
      const { boundaries, buckets } = state;
      const len = boundaries.length;

      // Linear search for small arrays (faster due to less overhead)
      if (len <= 8) {
        let i = 0;
        while (i < len && numValue > boundaries[i]) {
          i++;
        }
        buckets[i]++;
        return;
      }

      // Binary search for larger arrays
      let low = 0;
      let high = len;
      while (low < high) {
        const mid = (low + high) >>> 1;
        if (numValue > boundaries[mid]) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
      buckets[low]++;
    },
    finalize(state) {
      const buckets = [];
      for (let i = 0; i < state.boundaries.length; i++) {
        ArrayPrototypePush(buckets, {
          le: state.boundaries[i],
          count: state.buckets[i],
        });
      }
      ArrayPrototypePush(buckets, {
        le: Infinity,
        count: state.buckets[state.boundaries.length],
      });

      return {
        buckets,
        sum: state.sum,
        count: state.count,
        min: state.count > 0 ? state.min : undefined,
        max: state.count > 0 ? state.max : undefined,
      };
    },
    resetState(state) {
      for (let i = 0; i < state.buckets.length; i++) {
        state.buckets[i] = 0;
      }
      state.sum = 0;
      state.count = 0;
      state.min = Infinity;
      state.max = -Infinity;
    },
  },

  [kAggregatorSummary]: {
    createState(config) {
      const quantiles = config.quantiles ?? [0.5, 0.9, 0.95, 0.99];
      // Resolution multiplier for fractional value precision.
      // RecordableHistogram only supports integers, so we scale values up
      // before recording and scale quantiles back down when finalizing.
      // Default resolution of 1000 gives microsecond precision for ms values.
      const resolution = config.resolution ?? 1000;
      return {
        quantiles,
        resolution,
        histogram: createHistogram(),
        sum: 0,
        count: 0,
        // Track min/max separately with full precision (avoids HDRHistogram bucket rounding)
        min: Infinity,
        max: -Infinity,
      };
    },
    aggregate(state, value) {
      const numValue = typeof value === 'bigint' ? Number(value) : value;
      // Scale up by resolution to preserve fractional precision
      // e.g., 0.5ms * 1000 = 500 (stored as integer)
      const scaledValue = MathRound(numValue * state.resolution);
      // Clamp to valid range for RecordableHistogram (1 to 2^63-1)
      if (scaledValue >= 1) {
        state.histogram.record(scaledValue);
      }
      state.sum += numValue;
      state.count++;
      // Track exact min/max for full precision
      if (numValue < state.min) state.min = numValue;
      if (numValue > state.max) state.max = numValue;
    },
    finalize(state) {
      const quantileValues = {};
      const resolution = state.resolution;
      for (const q of state.quantiles) {
        // Scale back down to original units
        quantileValues[q] = state.histogram.percentile(q * 100) / resolution;
      }

      return {
        quantiles: quantileValues,
        sum: state.sum,
        count: state.count,
        // Use our exact min/max tracking (not HDRHistogram's bucket-rounded values)
        min: state.count > 0 ? state.min : undefined,
        max: state.count > 0 ? state.max : undefined,
      };
    },
    resetState(state) {
      state.histogram.reset();
      state.sum = 0;
      state.count = 0;
      state.min = Infinity;
      state.max = -Infinity;
    },
  },
};

/**
 * Get an aggregator by name or return custom aggregator object.
 */
function getAggregator(aggregation) {
  if (typeof aggregation === 'string') {
    const agg = builtinAggregators[aggregation];
    if (!agg) {
      throw new ERR_INVALID_ARG_VALUE(
        'aggregation',
        aggregation,
        'must be one of: sum, lastValue, histogram, summary',
      );
    }
    return agg;
  }
  // Custom aggregator object
  if (typeof aggregation === 'object' && aggregation !== null) {
    validateFunction(aggregation.createState, 'aggregation.createState');
    validateFunction(aggregation.aggregate, 'aggregation.aggregate');
    validateFunction(aggregation.finalize, 'aggregation.finalize');
    return aggregation;
  }
  throw new ERR_INVALID_ARG_TYPE('aggregation', ['string', 'object'], aggregation);
}

// =============================================================================
// Per-Metric Subscriber Model (like diagnostics_channel)
// =============================================================================

/**
 * Reservoir sampler using Algorithm R.
 * Maintains a fixed-size random sample of exemplars.
 * See: https://en.wikipedia.org/wiki/Reservoir_sampling
 */
class ReservoirSampler {
  #maxExemplars;
  #extract;
  #reservoir = [];
  #count = 0;

  constructor(maxExemplars, extract) {
    validateNumber(maxExemplars, 'maxExemplars', 1);
    validateFunction(extract, 'extract');
    this.#maxExemplars = maxExemplars;
    this.#extract = extract;
  }

  sample(value, timestamp, attributes) {
    // Extract trace context
    const context = this.#extract(attributes);
    if (!context || !context.traceId || !context.spanId) {
      return; // Skip if no valid trace context
    }

    const { traceId, spanId, filteredAttributes } = context;
    const exemplar = new Exemplar(value, timestamp, traceId, spanId, filteredAttributes);

    this.#count++;

    // Reservoir sampling: Algorithm R
    if (this.#reservoir.length < this.#maxExemplars) {
      // Fill reservoir
      ArrayPrototypePush(this.#reservoir, exemplar);
    } else {
      // Random replacement with decreasing probability
      const j = MathRound(MathRandom() * this.#count);
      if (j < this.#maxExemplars) {
        this.#reservoir[j] = exemplar;
      }
    }
  }

  getExemplars() {
    return this.#reservoir;
  }

  reset() {
    this.#reservoir = [];
    this.#count = 0;
  }
}

/**
 * Boundary sampler for histograms.
 * Maintains one exemplar per bucket boundary.
 */
class BoundarySampler {
  #boundaries;
  #extract;
  #exemplars; // SafeMap<bucketIndex, Exemplar>

  constructor(boundaries, extract) {
    if (!ArrayIsArray(boundaries)) {
      throw new ERR_INVALID_ARG_TYPE('boundaries', 'Array', boundaries);
    }
    validateFunction(extract, 'extract');
    this.#boundaries = boundaries;
    this.#extract = extract;
    this.#exemplars = new SafeMap();
  }

  sample(value, timestamp, attributes) {
    // Extract trace context
    const context = this.#extract(attributes);
    if (!context || !context.traceId || !context.spanId) {
      return; // Skip if no valid trace context
    }

    const { traceId, spanId, filteredAttributes } = context;

    // Find bucket index
    const numValue = typeof value === 'bigint' ? Number(value) : value;
    let bucketIndex = 0;
    const len = this.#boundaries.length;

    // Linear search for small arrays (faster due to less overhead)
    if (len <= 8) {
      while (bucketIndex < len && numValue > this.#boundaries[bucketIndex]) {
        bucketIndex++;
      }
    } else {
      // Binary search for larger arrays
      let low = 0;
      let high = len;
      while (low < high) {
        const mid = (low + high) >>> 1;
        if (numValue > this.#boundaries[mid]) {
          low = mid + 1;
        } else {
          high = mid;
        }
      }
      bucketIndex = low;
    }

    // Store or replace with 10% probability
    if (!this.#exemplars.has(bucketIndex) || MathRandom() < 0.1) {
      const exemplar = new Exemplar(value, timestamp, traceId, spanId, filteredAttributes);
      this.#exemplars.set(bucketIndex, exemplar);
    }
  }

  getExemplars() {
    return ArrayFrom(this.#exemplars.values());
  }

  reset() {
    this.#exemplars.clear();
  }
}

/**
 * Base class for metric subscribers.
 * Each subscriber encapsulates the state for one consumer watching one metric.
 * Subclasses implement different strategies for grouped vs ungrouped values.
 */
class MetricSubscriber {
  #descriptor;
  #aggregator;
  #temporality;

  constructor(descriptor, aggregator, config) {
    this.#descriptor = descriptor;
    this.#aggregator = aggregator;
    this.#temporality = config.temporality;
  }

  get descriptor() {
    return this.#descriptor;
  }

  get aggregator() {
    return this.#aggregator;
  }

  get temporality() {
    return this.#temporality;
  }

  /**
   * Get a snapshot of this subscriber's data.
   * @returns {object} Snapshot with descriptor, temporality, and dataPoints
   */
  getSnapshot() {
    return {
      descriptor: this.#descriptor.toJSON(),
      temporality: this.#temporality,
      dataPoints: this.getDataPoints(),
    };
  }

  // Subclasses must implement:
  // onValue(value, timestamp, attributes) - aggregate a value
  // getDataPoints() - return array of finalized data points
}

/**
 * Simple subscriber - all values go to a single state bucket.
 * Used when groupByAttributes=false (default, fastest path).
 * No attribute key lookup, no Map overhead.
 */
class SimpleMetricSubscriber extends MetricSubscriber {
  #state;
  #hasData = false;
  #exemplarSampler;

  constructor(descriptor, aggregator, config) {
    super(descriptor, aggregator, config);
    this.#state = aggregator.createState(config);
    this.#exemplarSampler = config.exemplar;
  }

  onValue(value, timestamp, attributes) {
    this.#hasData = true;
    this.aggregator.aggregate(this.#state, value, timestamp);

    // Sample for exemplar if sampler is configured
    if (this.#exemplarSampler) {
      this.#exemplarSampler.sample(value, timestamp, attributes);
    }
  }

  getDataPoints() {
    if (!this.#hasData) {
      return [];
    }
    const data = this.aggregator.finalize(this.#state);
    data.attributes = kEmptyObject;

    // Add exemplars if present
    if (this.#exemplarSampler) {
      const exemplars = this.#exemplarSampler.getExemplars();
      if (exemplars.length > 0) {
        data.exemplars = [];
        for (let i = 0; i < exemplars.length; i++) {
          ArrayPrototypePush(data.exemplars, exemplars[i].toJSON());
        }
      }
    }

    return [data];
  }

  /**
   * Reset state for delta temporality.
   * Called after collect() to reset aggregation state for the next interval.
   */
  reset() {
    if (this.temporality === 'delta') {
      this.#hasData = false;
      if (this.aggregator.resetState) {
        this.aggregator.resetState(this.#state);
      }
      // Reset exemplar sampler for delta temporality
      if (this.#exemplarSampler) {
        this.#exemplarSampler.reset();
      }
    }
  }
}

// Maximum number of unique attribute combinations per grouped subscriber
// Prevents unbounded memory growth from high-cardinality attributes
const kMaxCardinalityLimit = 2000;

/**
 * Grouped subscriber - values bucketed by attribute key.
 * Used when groupByAttributes=true.
 *
 * Implements cardinality limiting to prevent unbounded memory growth.
 * Eviction behavior differs by temporality:
 * - Delta: Evict oldest entries (data already exported)
 * - Cumulative: Drop new entries (preserve historical data integrity)
 */
class GroupedMetricSubscriber extends MetricSubscriber {
  #states; // Map<attrKey, { state, attributes, hasData }>
  #config;
  #cardinalityLimit;
  #cardinalityWarned = false; // Only warn once per subscriber
  #droppedCount = 0; // Count of dropped values for cumulative temporality
  #exemplarSampler;

  constructor(descriptor, aggregator, config) {
    super(descriptor, aggregator, config);
    this.#states = new SafeMap();
    this.#config = config;
    this.#cardinalityLimit = config.cardinalityLimit ?? kMaxCardinalityLimit;
    this.#exemplarSampler = config.exemplar;
  }

  onValue(value, timestamp, attributes) {
    const attrKey = this.#getAttributeKey(attributes);
    let entry = this.#states.get(attrKey);
    if (!entry) {
      // Enforce cardinality limit
      if (this.#states.size >= this.#cardinalityLimit) {
        // Warn on first limit hit
        if (!this.#cardinalityWarned) {
          this.#cardinalityWarned = true;
          const behavior = this.temporality === 'cumulative' ?
            'New attribute combinations are being dropped' :
            'Oldest attribute combinations are being evicted';
          process.emitWarning(
            `Metric '${this.descriptor.name}' reached cardinality limit of ${this.#cardinalityLimit}. ` +
            `${behavior}. ` +
            'Consider using groupBy or normalizeAttributes to reduce cardinality.',
            'MetricsWarning',
          );
        }

        // Different eviction strategies based on temporality
        if (this.temporality === 'cumulative') {
          // For cumulative: DROP the new value to preserve historical integrity
          // Evicting old cumulative data would cause incorrect sums when the key reappears
          this.#droppedCount++;
          return;
        }
        // For delta: EVICT oldest entry (its data was already exported)
        // Map iterates in insertion order, so first key is oldest
        const { value: oldestKey } = this.#states.keys().next();
        this.#states.delete(oldestKey);
      }
      entry = {
        state: this.aggregator.createState(this.#config),
        attributes: this.#normalizeAttributes(attributes),
        hasData: false,
      };
      this.#states.set(attrKey, entry);
    }
    entry.hasData = true;
    this.aggregator.aggregate(entry.state, value, timestamp);

    // Sample for exemplar if sampler is configured
    if (this.#exemplarSampler) {
      this.#exemplarSampler.sample(value, timestamp, attributes);
    }
  }

  /**
   * Get the count of dropped values due to cardinality limiting.
   * Only applicable for cumulative temporality.
   * @returns {number}
   */
  get droppedCount() {
    return this.#droppedCount;
  }

  getDataPoints() {
    const dataPoints = [];
    for (const { 1: entry } of this.#states) {
      if (!entry.hasData) continue;
      const data = this.aggregator.finalize(entry.state);
      data.attributes = entry.attributes;

      // Add exemplars if present (shared across all attribute groups)
      if (this.#exemplarSampler) {
        const exemplars = this.#exemplarSampler.getExemplars();
        if (exemplars.length > 0) {
          data.exemplars = [];
          for (let i = 0; i < exemplars.length; i++) {
            ArrayPrototypePush(data.exemplars, exemplars[i].toJSON());
          }
        }
      }

      ArrayPrototypePush(dataPoints, data);
    }
    return dataPoints;
  }

  /**
   * Reset state for delta temporality.
   * Called after collect() to reset aggregation state for the next interval.
   */
  reset() {
    if (this.temporality === 'delta') {
      for (const { 1: entry } of this.#states) {
        entry.hasData = false;
        if (this.aggregator.resetState) {
          this.aggregator.resetState(entry.state);
        }
      }
      // Reset exemplar sampler for delta temporality
      if (this.#exemplarSampler) {
        this.#exemplarSampler.reset();
      }
    }
  }

  #getAttributeKey(attributes) {
    // Custom key function
    if (this.#config.attributeKey) {
      return this.#config.attributeKey(attributes);
    }

    // If normalizeAttributes is configured, use normalized attrs for key
    if (this.#config.normalizeAttributes) {
      const normalized = this.#config.normalizeAttributes(attributes);
      return attributeIdentity.getId(normalized);
    }

    // Group by specific attributes
    if (this.#config.groupBy) {
      const grouped = {};
      for (const key of this.#config.groupBy) {
        if (key in attributes) {
          grouped[key] = attributes[key];
        }
      }
      return attributeIdentity.getId(grouped);
    }

    // Default: use all attributes
    return attributeIdentity.getId(attributes);
  }

  #normalizeAttributes(attributes) {
    if (this.#config.normalizeAttributes) {
      return this.#config.normalizeAttributes(attributes);
    }
    if (this.#config.groupBy) {
      const normalized = {};
      for (const key of this.#config.groupBy) {
        if (key in attributes) {
          normalized[key] = attributes[key];
        }
      }
      return normalized;
    }
    // Shallow copy to prevent external mutation of stored attributes
    return ObjectAssign({}, attributes);
  }
}

/**
 * Factory function to create the appropriate subscriber type.
 * @param {MetricDescriptor} descriptor - The metric descriptor
 * @param {object} aggregator - The aggregator to use
 * @param {object} config - The consumer's config for this metric
 * @returns {MetricSubscriber}
 */
function createMetricSubscriber(descriptor, aggregator, config) {
  if (config.groupByAttributes) {
    return new GroupedMetricSubscriber(descriptor, aggregator, config);
  }
  return new SimpleMetricSubscriber(descriptor, aggregator, config);
}

/**
 * A consumer that aggregates metric values using the subscriber model.
 *
 * Instead of receiving all values via onValue() and doing identity lookups,
 * each Consumer subscribes directly to the metrics it cares about (like
 * diagnostics_channel). This eliminates per-value "is this metric relevant?"
 * checks and moves that decision to subscription time.
 */
class Consumer {
  #config;
  #subscribers = [];      // Array of MetricSubscriber for iteration
  #observableSubscribers = []; // Array of { metric, subscriber } for observables
  #unsubscribeFns = [];   // Cleanup functions from metric._addSubscriber()
  #timestampMetrics = []; // Metrics where we enabled timestamps (for cleanup)
  #subscribedMetrics;     // Set of metric names we've subscribed to
  #pendingMetrics;        // Set of metric names we're waiting for
  #isWildcard;            // Whether this consumer wants all metrics
  #groupByAttributes;     // Whether to differentiate values by attributes
  #closed;
  #lastCollectTime;       // Timestamp of last collect() for delta startTime
  #autoCollectTimer;      // Timer for autoCollect()

  /**
   * @param {object} config
   * @param {string} [config.defaultAggregation='sum']
   * @param {string} [config.defaultTemporality='cumulative']
   * @param {boolean} [config.groupByAttributes=false] - Enable attribute differentiation
   * @param {object} [config.metrics] - Per-metric configuration
   */
  constructor(config = kEmptyObject) {
    validateObject(config, 'config');
    const metrics = config.metrics ?? kEmptyObject;
    const metricNames = ObjectKeys(metrics);

    // Validate exemplar samplers
    for (const metricName of metricNames) {
      const metricConfig = metrics[metricName];
      if (metricConfig?.exemplar) {
        const sampler = metricConfig.exemplar;
        if (typeof sampler !== 'object' || sampler === null) {
          throw new ERR_INVALID_ARG_TYPE(
            `config.metrics['${metricName}'].exemplar`,
            'object',
            sampler,
          );
        }
        validateFunction(sampler.sample, `config.metrics['${metricName}'].exemplar.sample`);
        validateFunction(sampler.getExemplars, `config.metrics['${metricName}'].exemplar.getExemplars`);
        validateFunction(sampler.reset, `config.metrics['${metricName}'].exemplar.reset`);
      }
    }

    this.#config = {
      defaultAggregation: config.defaultAggregation ?? kAggregatorSum,
      defaultTemporality: config.defaultTemporality ?? 'cumulative',
      metrics,
    };
    this.#groupByAttributes = config.groupByAttributes ?? false;
    this.#closed = false;
    this.#lastCollectTime = performance.now(); // Start time for first delta
    this.#autoCollectTimer = null;

    // Track subscriptions
    this.#subscribedMetrics = new SafeSet();

    // Wildcard mode: no specific metrics = subscribe to ALL metrics
    this.#isWildcard = metricNames.length === 0;

    if (this.#isWildcard) {
      // Subscribe to all existing metrics
      this.#pendingMetrics = null;
      for (const metric of registry.list()) {
        this.#subscribeToMetric(metric);
      }
    } else {
      // Subscribe to specific metrics that exist, track pending ones
      this.#pendingMetrics = new SafeSet(metricNames);
      for (const metricName of metricNames) {
        const metric = registry.get(metricName);
        if (metric) {
          this.#subscribeToMetric(metric);
          this.#pendingMetrics.delete(metricName);
        }
      }
    }

    // Register with registry to be notified of new metrics
    registry.addConsumer(this);
  }

  /**
   * Called by registry when a new metric is created.
   * @param {Metric} metric - The newly created metric
   */
  onMetricCreated(metric) {
    if (this.#closed) return;

    const name = metric.descriptor.name;

    // Wildcard: subscribe to all new metrics
    if (this.#isWildcard) {
      this.#subscribeToMetric(metric);
      return;
    }

    // Specific metrics: only subscribe if we're waiting for this one
    if (this.#pendingMetrics && this.#pendingMetrics.has(name)) {
      this.#subscribeToMetric(metric);
      this.#pendingMetrics.delete(name);
    }
  }

  /**
   * Called by registry when a metric is closed.
   * Cleans up subscriptions and state for the closed metric.
   * @param {Metric} metric - The closed metric
   */
  onMetricClosed(metric) {
    if (this.#closed) return;

    const name = metric.descriptor.name;

    // Remove from subscribed metrics
    this.#subscribedMetrics.delete(name);

    // Remove from observable subscribers
    for (let i = this.#observableSubscribers.length - 1; i >= 0; i--) {
      if (this.#observableSubscribers[i].metric === metric) {
        ArrayPrototypeSplice(this.#observableSubscribers, i, 1);
      }
    }

    // Remove subscriber for this metric
    // Note: The unsubscribe fn was already called when metric cleared its subscribers
    for (let i = this.#subscribers.length - 1; i >= 0; i--) {
      if (this.#subscribers[i].descriptor.name === name) {
        ArrayPrototypeSplice(this.#subscribers, i, 1);
      }
    }

    // Remove from timestamp metrics
    const tsIdx = ArrayPrototypeIndexOf(this.#timestampMetrics, metric);
    if (tsIdx !== -1) {
      ArrayPrototypeSplice(this.#timestampMetrics, tsIdx, 1);
    }
  }

  /**
   * Collect all metrics. Samples observables and returns aggregated state.
   * Returns an array of metric snapshots.
   * @returns {Array} Array of metric snapshots
   */
  collect() {
    if (this.#closed) return [];

    // Sample observable metrics - each subscriber samples only for itself
    // This maintains consumer isolation (no cross-consumer value leakage)
    const observableSubscribers = this.#observableSubscribers;
    for (let i = 0; i < observableSubscribers.length; i++) {
      const { metric, subscriber } = observableSubscribers[i];
      metric.sample(subscriber);
    }

    // Capture timestamps for this collection period
    const startTime = this.#lastCollectTime;
    const timestamp = performance.now();
    this.#lastCollectTime = timestamp;

    // Build snapshot - just iterate our subscribers
    const metrics = [];
    const subscribers = this.#subscribers;
    for (let i = 0; i < subscribers.length; i++) {
      const metricSnapshot = subscribers[i].getSnapshot();
      // Only include metrics that have data points
      if (metricSnapshot.dataPoints.length > 0) {
        metricSnapshot.timestamp = timestamp;
        // For delta temporality, include startTime to define the time window
        if (metricSnapshot.temporality === 'delta') {
          metricSnapshot.startTime = startTime;
        }
        ArrayPrototypePush(metrics, metricSnapshot);
      }
    }

    // Reset delta temporality subscribers after snapshot
    for (let i = 0; i < subscribers.length; i++) {
      subscribers[i].reset();
    }

    return metrics;
  }

  /**
   * Start automatic periodic collection.
   * @param {number} interval - Collection interval in milliseconds
   * @param {Function} callback - Called with each snapshot
   * @returns {Function} Stop function to cancel auto-collection
   */
  autoCollect(interval, callback) {
    validateNumber(interval, 'interval', 1);
    validateFunction(callback, 'callback');

    if (this.#closed) {
      throw new ERR_INVALID_STATE('Consumer is closed');
    }
    if (this.#autoCollectTimer) {
      throw new ERR_INVALID_STATE('autoCollect is already active');
    }

    this.#autoCollectTimer = setInterval(() => {
      try {
        const snapshot = this.collect();
        callback(snapshot);
      } catch (err) {
        triggerUncaughtException(err, false);
      }
    }, interval);

    // Don't keep process alive just for metrics collection
    this.#autoCollectTimer.unref();

    // Return stop function
    return () => {
      if (this.#autoCollectTimer) {
        clearInterval(this.#autoCollectTimer);
        this.#autoCollectTimer = null;
      }
    };
  }

  /**
   * Close this consumer and unregister from the registry.
   */
  close() {
    if (this.#closed) return;
    this.#closed = true;

    // Stop auto-collection if active
    if (this.#autoCollectTimer) {
      clearInterval(this.#autoCollectTimer);
      this.#autoCollectTimer = null;
    }

    // Unsubscribe from all metrics
    const unsubscribeFns = this.#unsubscribeFns;
    for (let i = 0; i < unsubscribeFns.length; i++) {
      unsubscribeFns[i]();
    }

    // Disable timestamp generation for metrics where we enabled it
    const timestampMetrics = this.#timestampMetrics;
    for (let i = 0; i < timestampMetrics.length; i++) {
      timestampMetrics[i][kDisableTimestamp]();
    }

    this.#subscribers = [];
    this.#observableSubscribers = [];
    this.#unsubscribeFns = [];
    this.#timestampMetrics = [];
    this.#subscribedMetrics.clear();
    if (this.#pendingMetrics) {
      this.#pendingMetrics.clear();
    }

    registry.removeConsumer(this);
  }

  #subscribeToMetric(metric) {
    const name = metric.descriptor.name;

    // Avoid duplicate subscriptions
    if (this.#subscribedMetrics.has(name)) {
      return;
    }
    this.#subscribedMetrics.add(name);

    const metricConfig = this.#getMetricConfig(metric.descriptor);
    const aggregator = getAggregator(metricConfig.aggregation);

    // Enable timestamp generation if this aggregator needs it
    if (aggregator.needsTimestamp) {
      metric[kEnableTimestamp]();
      ArrayPrototypePush(this.#timestampMetrics, metric);
    }

    // Factory creates the right subscriber type (Simple or Grouped)
    const subscriber = createMetricSubscriber(
      metric.descriptor,
      aggregator,
      metricConfig,
    );

    ArrayPrototypePush(this.#subscribers, subscriber);
    const unsubscribe = metric[kAddSubscriber](subscriber);
    ArrayPrototypePush(this.#unsubscribeFns, unsubscribe);

    // Track observable metrics separately for isolated sampling during collect()
    if (metric.isObservable) {
      ArrayPrototypePush(this.#observableSubscribers, { metric, subscriber });
    }
  }

  #getMetricConfig(descriptor) {
    const perMetric = this.#config.metrics[descriptor.name];
    return {
      aggregation: perMetric?.aggregation ?? this.#config.defaultAggregation,
      temporality: perMetric?.temporality ?? this.#config.defaultTemporality,
      monotonic: perMetric?.monotonic ?? false,
      boundaries: perMetric?.boundaries,
      quantiles: perMetric?.quantiles,
      groupBy: perMetric?.groupBy,
      attributeKey: perMetric?.attributeKey,
      normalizeAttributes: perMetric?.normalizeAttributes,
      cardinalityLimit: perMetric?.cardinalityLimit,
      groupByAttributes: this.#groupByAttributes,
      exemplar: perMetric?.exemplar,
    };
  }
}

// =============================================================================
// Global Registry
// =============================================================================

const kMetrics = Symbol('metrics');
const kObservables = Symbol('observables');
const kConsumers = Symbol('consumers');

/**
 * The global metric registry (MeterProvider equivalent in OTel terms).
 * Manages all metrics and their lifecycle.
 */
class MetricRegistry {
  [kMetrics] = new SafeMap();
  [kObservables] = []; // Direct array for fast iteration
  [kConsumers] = [];

  /**
   * Check if a metric with this name already exists (for singleton pattern).
   * @param {string} name - The metric name
   * @returns {Metric|undefined} The existing metric if found
   */
  get(name) {
    return this[kMetrics].get(name);
  }

  /**
   * Add a metric to the registry (internal, called by Metric constructor).
   * @param {Metric} metric - The metric to add
   */
  [kAdd](metric) {
    const name = metric.descriptor.name;
    this[kMetrics].set(name, metric);

    // Track observables in a direct array (no generator overhead)
    if (metric.isObservable) {
      ArrayPrototypePush(this[kObservables], metric);
    }

    // Notify existing consumers that a new metric was created
    // They can decide if they want to subscribe to it
    const consumers = this[kConsumers];
    for (let i = 0; i < consumers.length; i++) {
      const consumer = consumers[i];
      if (consumer.onMetricCreated) {
        consumer.onMetricCreated(metric);
      }
    }
  }

  /**
   * Remove a metric from the registry and notify consumers.
   * @param {Metric} metric - The metric to remove
   */
  [kRemove](metric) {
    this[kMetrics].delete(metric.descriptor.name);

    // Remove from observables list if it was observable
    if (metric.isObservable) {
      const idx = ArrayPrototypeIndexOf(this[kObservables], metric);
      if (idx !== -1) {
        ArrayPrototypeSplice(this[kObservables], idx, 1);
      }
    }

    // Notify consumers that a metric was closed
    // They can clean up their subscriptions and state
    const consumers = this[kConsumers];
    for (let i = 0; i < consumers.length; i++) {
      const consumer = consumers[i];
      if (consumer.onMetricClosed) {
        consumer.onMetricClosed(metric);
      }
    }
  }

  addConsumer(consumer) {
    ArrayPrototypePush(this[kConsumers], consumer);
  }

  removeConsumer(consumer) {
    const idx = ArrayPrototypeIndexOf(this[kConsumers], consumer);
    if (idx !== -1) {
      ArrayPrototypeSplice(this[kConsumers], idx, 1);
    }
  }

  /**
   * Get all observable metrics (direct array, no generator).
   * @returns {Array<Metric>}
   */
  observables() {
    return this[kObservables];
  }

  list() {
    return [...this[kMetrics].values()];
  }
}

const registry = new MetricRegistry();

// =============================================================================
// Public API
// =============================================================================

/**
 * Create a new metric or return existing one with the same name (singleton pattern).
 * @param {string} name - The metric name
 * @param {object} [options]
 * @param {string} [options.unit] - The unit of measurement
 * @param {string} [options.description] - Human-readable description
 * @param {Function} [options.observable] - Callback for observable metrics
 * @returns {Metric}
 */
function create(name, options = kEmptyObject) {
  // Check for existing metric first (singleton pattern)
  const existing = registry.get(name);
  if (existing) {
    // Warn if options differ from existing metric
    const desc = existing.descriptor;
    if ((options.unit !== undefined && options.unit !== desc.unit) ||
        (options.description !== undefined && options.description !== desc.description) ||
        (options.observable !== undefined && existing.isObservable !== (options.observable !== undefined))) {
      process.emitWarning(
        `Metric '${name}' already exists with different options. ` +
        'Returning existing metric. Options from this call are ignored.',
        'MetricsWarning',
      );
    }
    return existing;
  }
  return new Metric(name, options);
}

/**
 * Create a new consumer.
 *
 * Config can be passed in two formats:
 * 1. Direct metric configs at top level:
 *    { 'metric.name': { aggregation: 'sum' } }
 * 2. Nested under 'metrics' key (with optional defaults):
 *    { defaultAggregation: 'sum', metrics: { 'metric.name': { ... } } }
 * @param {object} [config]
 * @returns {Consumer}
 */
function createConsumer(config = kEmptyObject) {
  // If config has 'metrics' key, use it directly
  if (config.metrics) {
    return new Consumer(config);
  }

  // Otherwise, separate reserved keys from metric configs
  const { defaultAggregation, defaultTemporality, groupByAttributes, ...metrics } = config;
  const normalized = {
    defaultAggregation,
    defaultTemporality,
    groupByAttributes,
    metrics,
  };

  return new Consumer(normalized);
}

/**
 * List all registered metrics.
 * @returns {Array<Metric>}
 */
function list() {
  return registry.list();
}

/**
 * Get a metric by name.
 * @param {string} name
 * @returns {Metric|undefined}
 */
function get(name) {
  return registry.get(name);
}

// =============================================================================
// Optional: DiagnosticsChannel Consumer
// =============================================================================

/**
 * Special subscriber that publishes values to diagnostics_channel.
 * Unlike regular subscribers, this doesn't aggregate - just forwards.
 */
class DCMetricSubscriber {
  #descriptor;

  constructor(descriptor) {
    this.#descriptor = descriptor;
  }

  onValue(value, timestamp, attributes) {
    const ch = this.#descriptor.channel;
    if (ch.hasSubscribers) {
      // Create entry object only when there are subscribers
      ch.publish({
        descriptor: this.#descriptor,
        value,
        attributes,
        timestamp,
      });
    }
  }
}

let dcConsumer = null;

/**
 * Create a singleton consumer that forwards all values to diagnostics_channel.
 * Uses the subscriber model to receive values directly from metrics.
 * @returns {object}
 */
function createDiagnosticsChannelConsumer() {
  if (dcConsumer) return dcConsumer;

  const unsubscribeFns = [];
  const timestampMetrics = []; // Track metrics where we enabled timestamps
  const subscribedMetrics = new SafeSet();
  const observableSubscribers = []; // Track { metric, subscriber } for observables

  function subscribeToMetric(metric) {
    const name = metric.descriptor.name;
    if (subscribedMetrics.has(name)) return;
    subscribedMetrics.add(name);

    // DC consumer needs timestamps
    metric[kEnableTimestamp]();
    ArrayPrototypePush(timestampMetrics, metric);

    const subscriber = new DCMetricSubscriber(metric.descriptor);
    const unsubscribe = metric[kAddSubscriber](subscriber);
    ArrayPrototypePush(unsubscribeFns, unsubscribe);

    // Track observable metrics for isolated sampling
    if (metric.isObservable) {
      ArrayPrototypePush(observableSubscribers, { metric, subscriber });
    }
  }

  // Subscribe to all existing metrics
  for (const metric of registry.list()) {
    subscribeToMetric(metric);
  }

  dcConsumer = {
    // Called by registry when a new metric is created
    onMetricCreated(metric) {
      subscribeToMetric(metric);
    },
    // Called by registry when a metric is closed
    onMetricClosed(metric) {
      const name = metric.descriptor.name;
      subscribedMetrics.delete(name);

      // Remove from observable subscribers
      for (let i = observableSubscribers.length - 1; i >= 0; i--) {
        if (observableSubscribers[i].metric === metric) {
          ArrayPrototypeSplice(observableSubscribers, i, 1);
        }
      }

      // Remove from timestamp metrics
      const tsIdx = ArrayPrototypeIndexOf(timestampMetrics, metric);
      if (tsIdx !== -1) {
        ArrayPrototypeSplice(timestampMetrics, tsIdx, 1);
      }
    },
    collect() {
      // Sample observables - each subscriber samples only for itself
      for (let i = 0; i < observableSubscribers.length; i++) {
        const { metric, subscriber } = observableSubscribers[i];
        metric.sample(subscriber);
      }
      return null;
    },
    close() {
      // Unsubscribe from all metrics
      for (let i = 0; i < unsubscribeFns.length; i++) {
        unsubscribeFns[i]();
      }
      // Disable timestamp generation for metrics where we enabled it
      for (let i = 0; i < timestampMetrics.length; i++) {
        timestampMetrics[i][kDisableTimestamp]();
      }
      unsubscribeFns.length = 0;
      timestampMetrics.length = 0;
      observableSubscribers.length = 0;
      subscribedMetrics.clear();
      registry.removeConsumer(this);
      dcConsumer = null;
    },
  };

  registry.addConsumer(dcConsumer);
  return dcConsumer;
}

// =============================================================================
// Exports
// =============================================================================

module.exports = {
  // Classes
  MetricDescriptor,
  Metric,
  Timer,
  Consumer,
  InstrumentationScope,
  Exemplar,
  ReservoirSampler,
  BoundarySampler,

  // Factory functions
  create,
  createConsumer,
  createDiagnosticsChannelConsumer,

  // Utilities
  list,
  get,
};

// Getter for diagnosticsChannelConsumer singleton (returns null if not created)
ObjectDefineProperty(module.exports, 'diagnosticsChannelConsumer', {
  __proto__: null,
  configurable: false,
  enumerable: true,
  get() {
    return dcConsumer;
  },
});
