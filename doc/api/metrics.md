# Metrics

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

<!-- source_link=lib/internal/perf/metrics.js -->

The `node:perf_hooks` metrics API provides a flexible, low-overhead
instrumentation system for application performance monitoring. It follows a
three-layer architecture: value producers generate values via direct recording,
timers, or observable callbacks; descriptors identify and group metric values;
consumers subscribe to metrics and aggregate values into snapshots.

The metrics API can be accessed using:

```mjs
import { metrics } from 'node:perf_hooks';
```

```cjs
const { metrics } = require('node:perf_hooks');
```

## Public API

### Overview

```mjs
import { metrics } from 'node:perf_hooks';

// Create a metric (singleton — same name returns same instance)
const requests = metrics.create('http.requests', { unit: '{count}' });

// Create a consumer to aggregate values
const consumer = metrics.createConsumer({
  defaultAggregation: 'sum',
  groupByAttributes: true,
  metrics: {
    'http.requests': { aggregation: 'sum' },
  },
});

// Record values with optional attributes
requests.record(1, { method: 'GET', status: 200 });
requests.record(1, { method: 'POST', status: 201 });

// Collect aggregated snapshots
const snapshot = consumer.collect();
console.log(snapshot[0].dataPoints);
// [
//   { sum: 1, count: 1, attributes: { method: 'GET', status: 200 } },
//   { sum: 1, count: 1, attributes: { method: 'POST', status: 201 } },
// ]

consumer.close();
requests.close();
```

```cjs
const { metrics } = require('node:perf_hooks');

const requests = metrics.create('http.requests', { unit: '{count}' });

const consumer = metrics.createConsumer({
  defaultAggregation: 'sum',
  groupByAttributes: true,
  metrics: {
    'http.requests': { aggregation: 'sum' },
  },
});

requests.record(1, { method: 'GET', status: 200 });
requests.record(1, { method: 'POST', status: 201 });

const snapshot = consumer.collect();
console.log(snapshot[0].dataPoints);

consumer.close();
requests.close();
```

#### `metrics.create(name[, options])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The metric name. Must not be empty.
* `options` {Object}
  * `unit` {string} The unit of measurement (e.g., `'ms'`, `'By'`,
    `'{count}'`).
  * `description` {string} Human-readable description.
  * `observable` {Function} If provided, makes the metric observable. The
    function is called during [`consumer.collect()`][] with a facade object
    as its argument. The facade has:
    * `record(value[, attributes])` — Records a value for the current
      subscriber.
    * `descriptor` — The [`MetricDescriptor`][] for this metric.
  * `scope` {InstrumentationScope} The instrumentation scope.
* Returns: {Metric}

Creates a new metric or returns the existing one with the same name. If a
metric with the same name already exists but with different options, a
`'MetricsWarning'` process warning is emitted and the existing metric is
returned unchanged.

Each metric maintains its own subscriber list. Values are dispatched directly
to subscribed consumers with no per-value identity lookups, similar to
`node:diagnostics_channel`.

```mjs
import { metrics } from 'node:perf_hooks';
import { memoryUsage } from 'node:process';

// Direct metric — record values explicitly
const duration = metrics.create('http.request.duration', {
  unit: 'ms',
  description: 'HTTP request duration in milliseconds',
});

duration.record(42, { route: '/api/users' });

// Observable metric — sampled on demand during collect()
const memory = metrics.create('process.memory.heap', {
  unit: 'By',
  observable: (metric) => {
    const mem = memoryUsage();
    metric.record(mem.heapUsed, { type: 'used' });
    metric.record(mem.heapTotal, { type: 'total' });
  },
});
```

```cjs
const { metrics } = require('node:perf_hooks');
const { memoryUsage } = require('node:process');

const duration = metrics.create('http.request.duration', {
  unit: 'ms',
  description: 'HTTP request duration in milliseconds',
});

duration.record(42, { route: '/api/users' });

const memory = metrics.create('process.memory.heap', {
  unit: 'By',
  observable: (metric) => {
    const mem = memoryUsage();
    metric.record(mem.heapUsed, { type: 'used' });
    metric.record(mem.heapTotal, { type: 'total' });
  },
});
```

#### `metrics.createConsumer([config])`

<!-- YAML
added: REPLACEME
-->

* `config` {Object}
  * `defaultAggregation` {string} Default aggregation strategy for all
    metrics. One of `'sum'`, `'lastValue'`, `'histogram'`, `'summary'`, or a
    custom aggregator object. **Default:** `'sum'`.
  * `defaultTemporality` {string} Default temporality. `'cumulative'` means
    data points represent totals since metric creation; `'delta'` means
    data points represent values since the last [`consumer.collect()`][]
    call and state is reset after collection. **Default:** `'cumulative'`.
  * `groupByAttributes` {boolean} When `true`, values are bucketed by their
    attribute combinations. When `false`, all values aggregate into a single
    bucket regardless of attributes. **Default:** `false`.
  * `metrics` {Object} Per-metric configuration keyed by metric name. Each
    value is an object with:
    * `aggregation` {string|Object} Aggregation strategy for this metric.
      Built-in strategies:
      * `'sum'` — Running sum and count. Supports `monotonic: true` to
        reject negative values. Data point fields: `sum`, `count`.
      * `'lastValue'` — Most recent value and its timestamp. Data point
        fields: `value`, `timestamp`.
      * `'histogram'` — Explicit-boundary histogram. Data point fields:
        `buckets` (array of `{ le, count }`), `sum`, `count`, `min`, `max`.
      * `'summary'` — Quantile summary. Data point fields: `quantiles`
        (object mapping quantile to value), `sum`, `count`, `min`, `max`.
      * Custom object with `createState(config)`, `aggregate(state, value[,
        timestamp])`, `finalize(state)`, and optional `resetState(state)` and
        `needsTimestamp` properties.
    * `temporality` {string} Overrides `defaultTemporality` for this metric.
    * `monotonic` {boolean} For `'sum'` aggregation, reject negative values.
    * `boundaries` {number\[]} For `'histogram'` aggregation, bucket
      boundaries. **Default:** `[10, 50, 100, 500, 1000]`.
    * `quantiles` {number\[]} For `'summary'` aggregation, quantile values.
      **Default:** `[0.5, 0.9, 0.95, 0.99]`.
    * `groupBy` {string\[]} Attribute keys to group by (subset of all
      attributes).
    * `normalizeAttributes` {Function} Function to normalize attribute
      objects before grouping.
    * `attributeKey` {Function} Custom function to derive the grouping key
      from an attributes object.
    * `cardinalityLimit` {number} Maximum unique attribute combinations when
      `groupByAttributes` is enabled. Oldest entries are evicted for
      `'delta'` temporality; new entries are dropped for `'cumulative'`
      temporality. A `'MetricsWarning'` is emitted on first limit hit.
      **Default:** `2000`.
    * `exemplar` {Object} Exemplar sampler. Must implement three methods:
      `sample(value, timestamp, attributes)` called on each recorded value,
      `getExemplars()` returning an {Exemplar\[]} array, and `reset()` called
      after collection to clear stored exemplars. See [`ReservoirSampler`][]
      and [`BoundarySampler`][] for built-in implementations.
* Returns: {Consumer}

Creates a consumer that aggregates metric values. The consumer subscribes to
existing metrics immediately and to metrics created later that match its
configuration. A wildcard consumer (no `metrics` key, or empty `metrics`)
subscribes to all current and future metrics.

Config keys that are not reserved (`defaultAggregation`, `defaultTemporality`,
`groupByAttributes`, `metrics`) are treated as metric names in the shorthand
format:

```mjs
import { metrics } from 'node:perf_hooks';

// Shorthand: top-level keys are metric names
const consumer = metrics.createConsumer({
  'http.requests': { aggregation: 'sum' },
  'http.duration': { aggregation: 'histogram' },
});

// Explicit: nested under 'metrics' key
const consumer2 = metrics.createConsumer({
  defaultAggregation: 'sum',
  groupByAttributes: true,
  metrics: {
    'http.requests': { aggregation: 'sum' },
    'http.duration': { aggregation: 'histogram' },
  },
});

// Wildcard: subscribes to all metrics
const wildcard = metrics.createConsumer();

consumer.close();
consumer2.close();
wildcard.close();
```

```cjs
const { metrics } = require('node:perf_hooks');

const consumer = metrics.createConsumer({
  'http.requests': { aggregation: 'sum' },
  'http.duration': { aggregation: 'histogram' },
});

const consumer2 = metrics.createConsumer({
  defaultAggregation: 'sum',
  groupByAttributes: true,
  metrics: {
    'http.requests': { aggregation: 'sum' },
    'http.duration': { aggregation: 'histogram' },
  },
});

const wildcard = metrics.createConsumer();

consumer.close();
consumer2.close();
wildcard.close();
```

#### `metrics.createDiagnosticsChannelConsumer()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object}

Creates a singleton consumer that forwards all metric values to
`node:diagnostics_channel`. Each metric publishes to a channel named
`metrics:{name}`. Values are only published when the channel has active
subscribers.

Calling this function again after the first call returns the same instance.
The returned object has `collect()` (to sample observable metrics) and
`close()` methods.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel';
import { metrics } from 'node:perf_hooks';

diagnosticsChannel.subscribe('metrics:http.requests', (msg) => {
  // msg.descriptor, msg.value, msg.attributes, msg.timestamp
  console.log(msg.value, msg.attributes);
});

metrics.createDiagnosticsChannelConsumer();

const m = metrics.create('http.requests');
m.record(1, { method: 'GET' }); // Published to the channel immediately
```

```cjs
const diagnosticsChannel = require('node:diagnostics_channel');
const { metrics } = require('node:perf_hooks');

diagnosticsChannel.subscribe('metrics:http.requests', (msg) => {
  console.log(msg.value, msg.attributes);
});

metrics.createDiagnosticsChannelConsumer();

const m = metrics.create('http.requests');
m.record(1, { method: 'GET' });
```

#### `metrics.get(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {Metric|undefined}

Returns the registered metric with the given name, or `undefined` if not
found.

#### `metrics.list()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Metric\[]}

Returns an array of all currently registered metrics.

#### `metrics.diagnosticsChannelConsumer`

<!-- YAML
added: REPLACEME
-->

* {Object|null}

The diagnostics channel consumer singleton. `null` if
[`metrics.createDiagnosticsChannelConsumer()`][] has not been called.

### Class: `Metric`

<!-- YAML
added: REPLACEME
-->

A metric records values and dispatches them immediately to all subscribed
consumers. Metrics are created with [`metrics.create()`][], which implements a
singleton pattern — creating a metric with an already-registered name returns
the existing instance.

#### `metric.descriptor`

<!-- YAML
added: REPLACEME
-->

* {MetricDescriptor}

The immutable descriptor for this metric.

#### `metric.isObservable`

<!-- YAML
added: REPLACEME
-->

* {boolean}

`true` if this metric has an observable callback.

#### `metric.isClosed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

`true` if this metric has been closed.

#### `metric.record(value[, attributes[, timestamp]])`

<!-- YAML
added: REPLACEME
-->

* `value` {number|bigint} The value to record.
* `attributes` {Object} Attributes for this value. **Default:** `{}`.
* `timestamp` {number} Optional timestamp override.

Records a value and dispatches it to all subscribed consumers. Validation is
always performed. If there are no subscribers, the value is not stored.

```mjs
import { metrics } from 'node:perf_hooks';

const m = metrics.create('db.query.duration', { unit: 'ms' });

m.record(12.5, { db: 'postgres', operation: 'SELECT' });
m.record(3n);         // BigInt values are supported
m.record(0.5);
```

```cjs
const { metrics } = require('node:perf_hooks');

const m = metrics.create('db.query.duration', { unit: 'ms' });

m.record(12.5, { db: 'postgres', operation: 'SELECT' });
m.record(3n);
m.record(0.5);
```

#### `metric.startTimer([attributes])`

<!-- YAML
added: REPLACEME
-->

* `attributes` {Object} Attributes to include with the recorded duration.
  **Default:** `{}`.
* Returns: {Timer}

Creates a {Timer} that records its duration to this metric when stopped.
Timers are pooled for reuse to reduce garbage collection pressure.

```mjs
import { metrics } from 'node:perf_hooks';

const duration = metrics.create('http.request.duration', { unit: 'ms' });

// Manual stop
const timer = duration.startTimer({ route: '/api/users' });
// ... handle request ...
const ms = timer.stop(); // Records duration and returns it

// Automatic stop using `using`
{
  using t = duration.startTimer({ route: '/api/orders' });
  // ... handle request ...
  // Timer is stopped automatically at end of block
}
```

```cjs
const { metrics } = require('node:perf_hooks');

const duration = metrics.create('http.request.duration', { unit: 'ms' });

const timer = duration.startTimer({ route: '/api/users' });
// ... handle request ...
const ms = timer.stop();
```

#### `metric.close()`

<!-- YAML
added: REPLACEME
-->

Unregisters the metric from the global registry and notifies all consumers
via [`consumer.onMetricClosed()`][]. After closing, `record()` calls are
silently ignored (but still validated) and consumers receive no further values.
Calling `close()` multiple times is safe. After closing, a new metric can be
created with the same name.

### Class: `Timer`

<!-- YAML
added: REPLACEME
-->

A helper for measuring durations. Obtained via [`metric.startTimer()`][].

#### `timer.startTime`

<!-- YAML
added: REPLACEME
-->

* {number}

The start time in milliseconds from `performance.now()`.

#### `timer.stop()`

<!-- YAML
added: REPLACEME
-->

* Returns: {number} The duration in milliseconds.

Stops the timer and records the duration to the associated metric. Throws
`ERR_INVALID_STATE` if called after the timer has already been stopped.

#### `timer[Symbol.dispose]()`

<!-- YAML
added: REPLACEME
-->

Stops the timer if it has not already been stopped. Enables `using` syntax
for automatic cleanup.

### Class: `Consumer`

<!-- YAML
added: REPLACEME
-->

Aggregates metric values using a subscriber model. Each consumer subscribes
directly to metrics at subscription time, eliminating per-value identity
lookups. Consumers are created with [`metrics.createConsumer()`][].

#### `consumer.collect()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object\[]}

Collects all metrics and returns an array of metric snapshots. Observable
metrics are sampled during this call, each receiving only its own subscriber
to maintain consumer isolation.

Each snapshot is an object with:

* `descriptor` {Object} The metric descriptor (`name`, `unit`, `description`,
  `scope`).
* `temporality` {string} `'cumulative'` or `'delta'`.
* `timestamp` {number} The collection timestamp.
* `startTime` {number} Start of the time window (`'delta'` temporality only).
* `dataPoints` {Object\[]} Array of aggregated data points. Each has an
  `attributes` property plus aggregation-specific fields (see
  [`metrics.createConsumer()`][]).

Snapshots with no data points are omitted. For `'delta'` temporality,
subscriber state is reset after collection.

```mjs
import { metrics } from 'node:perf_hooks';

const m = metrics.create('app.requests');
const consumer = metrics.createConsumer({
  groupByAttributes: true,
  metrics: { 'app.requests': { aggregation: 'sum' } },
});

m.record(1, { status: 200 });
m.record(1, { status: 404 });

const snapshot = consumer.collect();
console.log(snapshot[0].dataPoints);
// [
//   { sum: 1, count: 1, attributes: { status: 200 } },
//   { sum: 1, count: 1, attributes: { status: 404 } },
// ]

consumer.close();
m.close();
```

```cjs
const { metrics } = require('node:perf_hooks');

const m = metrics.create('app.requests');
const consumer = metrics.createConsumer({
  groupByAttributes: true,
  metrics: { 'app.requests': { aggregation: 'sum' } },
});

m.record(1, { status: 200 });
m.record(1, { status: 404 });

const snapshot = consumer.collect();
console.log(snapshot[0].dataPoints);

consumer.close();
m.close();
```

#### `consumer.autoCollect(interval, callback)`

<!-- YAML
added: REPLACEME
-->

* `interval` {number} Collection interval in milliseconds.
* `callback` {Function} Called with the snapshot array from each collection.
* Returns: {Function} A stop function that cancels auto-collection.

Starts periodic automatic collection. The underlying timer is unref'd so it
does not keep the process alive. Throws `ERR_INVALID_STATE` if auto-collection
is already active or if the consumer is closed.

```mjs
import { metrics } from 'node:perf_hooks';

const consumer = metrics.createConsumer();

const stop = consumer.autoCollect(10_000, (snapshot) => {
  // Called every 10 seconds with the collected snapshot
  for (const metric of snapshot) {
    console.log(metric.descriptor.name, metric.dataPoints);
  }
});

// Later, cancel periodic collection
stop();
consumer.close();
```

```cjs
const { metrics } = require('node:perf_hooks');

const consumer = metrics.createConsumer();

const stop = consumer.autoCollect(10_000, (snapshot) => {
  for (const metric of snapshot) {
    console.log(metric.descriptor.name, metric.dataPoints);
  }
});

stop();
consumer.close();
```

#### `consumer.close()`

<!-- YAML
added: REPLACEME
-->

Closes the consumer: stops auto-collection, unsubscribes from all metrics,
and unregisters from the global registry. Safe to call multiple times.

#### `consumer.onMetricCreated(metric)`

<!-- YAML
added: REPLACEME
-->

* `metric` {Metric}

Called by the registry when a new metric is created. Override to observe
metric creation events. The default implementation subscribes to the metric
if it matches the consumer's configuration.

#### `consumer.onMetricClosed(metric)`

<!-- YAML
added: REPLACEME
-->

* `metric` {Metric}

Called by the registry when a metric is closed. Override to observe metric
closure events. The default implementation cleans up the consumer's
subscriptions for that metric.

### Class: `MetricDescriptor`

<!-- YAML
added: REPLACEME
-->

An immutable descriptor for a metric. Created once per metric and reused.
Consumers can use reference equality (`===`) for fast comparisons. Obtained
via [`metric.descriptor`][].

#### `metricDescriptor.name`

<!-- YAML
added: REPLACEME
-->

* {string}

The metric name.

#### `metricDescriptor.unit`

<!-- YAML
added: REPLACEME
-->

* {string|undefined}

The unit of measurement.

#### `metricDescriptor.description`

<!-- YAML
added: REPLACEME
-->

* {string|undefined}

The human-readable description.

#### `metricDescriptor.scope`

<!-- YAML
added: REPLACEME
-->

* {InstrumentationScope|undefined}

The instrumentation scope.

#### `metricDescriptor.channel`

<!-- YAML
added: REPLACEME
-->

* {Channel}

The `node:diagnostics_channel` channel for this metric (lazily created). The
channel name is `metrics:{name}`.

#### `metricDescriptor.toJSON()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object}

Returns a plain object representation suitable for JSON serialization.

### Class: `InstrumentationScope`

<!-- YAML
added: REPLACEME
-->

Identifies the library or module producing metrics. Corresponds to the
[OpenTelemetry Instrumentation Scope][] concept.

#### `new InstrumentationScope(name[, version[, schemaUrl]])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The scope name (e.g., package name).
* `version` {string} The scope version.
* `schemaUrl` {string} The schema URL.

#### `instrumentationScope.name`

<!-- YAML
added: REPLACEME
-->

* {string}

#### `instrumentationScope.version`

<!-- YAML
added: REPLACEME
-->

* {string|undefined}

#### `instrumentationScope.schemaUrl`

<!-- YAML
added: REPLACEME
-->

* {string|undefined}

#### `instrumentationScope.toJSON()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object}

### Class: `Exemplar`

<!-- YAML
added: REPLACEME
-->

A sample measurement with trace context, used to correlate metric data points
with distributed traces. See the [OpenTelemetry Exemplars][] specification.
`Exemplar` instances are returned by [`reservoirSampler.getExemplars()`][] and
[`boundarySampler.getExemplars()`][].

#### `exemplar.value`

<!-- YAML
added: REPLACEME
-->

* {number|bigint}

#### `exemplar.timestamp`

<!-- YAML
added: REPLACEME
-->

* {number}

#### `exemplar.traceId`

<!-- YAML
added: REPLACEME
-->

* {string}

#### `exemplar.spanId`

<!-- YAML
added: REPLACEME
-->

* {string}

#### `exemplar.filteredAttributes`

<!-- YAML
added: REPLACEME
-->

* {Object}

#### `exemplar.toJSON()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object}

### Class: `ReservoirSampler`

<!-- YAML
added: REPLACEME
-->

An exemplar sampler using reservoir sampling (Algorithm R). Maintains a
fixed-size random sample of exemplars over the collection window.

#### `new metrics.ReservoirSampler(maxExemplars, extract)`

<!-- YAML
added: REPLACEME
-->

* `maxExemplars` {number} Maximum number of exemplars to retain. Must be
  at least `1`.
* `extract` {Function} Called with the recording attributes to extract trace
  context. Must return an object with `traceId`, `spanId`, and
  `filteredAttributes` properties, or `null` to skip sampling.

```mjs
import { metrics } from 'node:perf_hooks';

const { ReservoirSampler } = metrics;

function extractTraceContext(attributes) {
  if (!attributes.traceId || !attributes.spanId) return null;
  const { traceId, spanId, ...filteredAttributes } = attributes;
  return { traceId, spanId, filteredAttributes };
}

const sampler = new ReservoirSampler(10, extractTraceContext);

const consumer = metrics.createConsumer({
  metrics: {
    'http.request.duration': {
      aggregation: 'histogram',
      exemplar: sampler,
    },
  },
});
```

#### `reservoirSampler.sample(value, timestamp, attributes)`

<!-- YAML
added: REPLACEME
-->

* `value` {number|bigint}
* `timestamp` {number}
* `attributes` {Object}

Records a candidate exemplar. If the reservoir is not full, the sample is
added directly. Otherwise, it randomly replaces an existing sample with
decreasing probability (Algorithm R).

#### `reservoirSampler.getExemplars()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Exemplar\[]}

Returns the current set of sampled exemplars.

#### `reservoirSampler.reset()`

<!-- YAML
added: REPLACEME
-->

Clears all exemplars and resets the sample count. Called automatically after
`'delta'` temporality collection.

### Class: `BoundarySampler`

<!-- YAML
added: REPLACEME
-->

An exemplar sampler that maintains one exemplar per histogram bucket boundary.
Suitable for use with `'histogram'` aggregation when you want one representative
trace per bucket.

#### `new metrics.BoundarySampler(boundaries, extract)`

<!-- YAML
added: REPLACEME
-->

* `boundaries` {number\[]} Histogram bucket boundaries. Should match the
  `boundaries` option passed to the consumer for the same metric.
* `extract` {Function} Called with the recording attributes to extract trace
  context. Must return an object with `traceId`, `spanId`, and
  `filteredAttributes` properties, or `null` to skip sampling.

```mjs
import { metrics } from 'node:perf_hooks';

const { BoundarySampler } = metrics;

function extractTraceContext(attributes) {
  if (!attributes.traceId || !attributes.spanId) return null;
  const { traceId, spanId, ...filteredAttributes } = attributes;
  return { traceId, spanId, filteredAttributes };
}

const boundaries = [10, 50, 100, 500];
const sampler = new BoundarySampler(boundaries, extractTraceContext);

const consumer = metrics.createConsumer({
  metrics: {
    'http.request.duration': {
      aggregation: 'histogram',
      boundaries,
      exemplar: sampler,
    },
  },
});
```

#### `boundarySampler.sample(value, timestamp, attributes)`

<!-- YAML
added: REPLACEME
-->

* `value` {number|bigint}
* `timestamp` {number}
* `attributes` {Object}

Records a candidate exemplar into the bucket corresponding to `value`. Each
bucket retains only its most recently sampled exemplar.

#### `boundarySampler.getExemplars()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Exemplar\[]}

Returns exemplars for all populated buckets.

#### `boundarySampler.reset()`

<!-- YAML
added: REPLACEME
-->

Clears all stored exemplars. Called automatically after `'delta'` temporality
collection.

[OpenTelemetry Exemplars]: https://opentelemetry.io/docs/specs/otel/metrics/data-model/#exemplars
[OpenTelemetry Instrumentation Scope]: https://opentelemetry.io/docs/specs/otel/glossary/#instrumentation-scope
[`BoundarySampler`]: #class-boundarysampler
[`MetricDescriptor`]: #class-metricdescriptor
[`ReservoirSampler`]: #class-reservoirsampler
[`boundarySampler.getExemplars()`]: #boundarysamplergetexemplars
[`consumer.collect()`]: #consumercollect
[`consumer.onMetricClosed()`]: #consumeronmetricclosedmetric
[`metric.descriptor`]: #metricdescriptor
[`metric.startTimer()`]: #metricstarttimerattributes
[`metrics.create()`]: #metricscreatenameoptions
[`metrics.createConsumer()`]: #metricscreateConsumerconfig
[`metrics.createDiagnosticsChannelConsumer()`]: #metricscreatediagnosticschannelconsumer
[`reservoirSampler.getExemplars()`]: #reservoirsamplergetexemplars
