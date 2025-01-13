# Metrics

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

<!-- source_link=lib/metrics.js -->

The `node:metrics` module provides an API for application instrumentation and
performance monitoring. It offers various metric types and built-in exporters
for popular monitoring systems.

The module can be accessed using:

```mjs
import * as metrics from 'node:metrics';
```

```cjs
const metrics = require('node:metrics');
```

## Overview

The metrics API enables developers to instrument their applications with custom
metrics that can be collected and exported to monitoring systems. All metrics
publish their data through the `node:diagnostics_channel` module, allowing for
flexible consumption patterns.

### Example

```mjs
import { counter, timer, statsdStream } from 'node:metrics';
import { createWriteStream } from 'node:fs';

// Create a counter metric
const apiCalls = counter('api.calls', { service: 'web' });

// Create a timer factory
const requestTimer = timer('api.request.duration', { service: 'web' });

// Export metrics to StatsD format
const statsd = statsdStream();
statsd.pipe(createWriteStream('metrics.log'));

// Use metrics in your application
function handleRequest(req, res) {
  const timer = requestTimer.create({ endpoint: req.url });

  apiCalls.increment();

  // Process request...

  timer.stop();
}
```

```cjs
const { counter, timer, statsdStream } = require('node:metrics');
const { createWriteStream } = require('node:fs');

// Create a counter metric
const apiCalls = counter('api.calls', { service: 'web' });

// Create a timer factory
const requestTimer = timer('api.request.duration', { service: 'web' });

// Export metrics to StatsD format
const statsd = statsdStream();
statsd.pipe(createWriteStream('metrics.log'));

// Use metrics in your application
function handleRequest(req, res) {
  const timer = requestTimer.create({ endpoint: req.url });

  apiCalls.increment();

  // Process request...

  timer.stop();
}
```

## Metric Types

### `metrics.counter(name[, meta])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the counter metric.
* `meta` {Object} Optional metadata to attach to all reports.
* Returns: {metrics.Counter}

Creates a counter metric that tracks cumulative values.

```mjs
import { counter } from 'node:metrics';

const errorCount = counter('errors.total', { component: 'database' });

errorCount.increment();     // Increment by 1
errorCount.increment(5);    // Increment by 5
errorCount.decrement(2);    // Decrement by 2
```


### `metrics.gauge(name[, meta])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the gauge metric.
* `meta` {Object} Optional metadata to attach to all reports.
* Returns: {metrics.Gauge}

Creates a gauge metric that represents a single value at a point in time.

```mjs
import { gauge } from 'node:metrics';
import { memoryUsage } from 'node:process';

const memory = gauge('memory.usage.bytes');

memory.reset(memoryUsage().heapUsed);
memory.applyDelta(1024);  // Add 1024 to current value
```

### `metrics.meter(name, interval[, meta])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the meter metric.
* `interval` {number} The time window in milliseconds for rate calculation.
* `meta` {Object} Optional metadata to attach to all reports.
* Returns: {metrics.Meter}

Creates a meter metric that measures the rate of events over time.

```mjs
import { meter } from 'node:metrics';

const requestRate = meter('requests.rate', 60000); // 1 minute window

requestRate.mark();     // Mark one event
requestRate.mark(10);   // Mark 10 events
```

### `metrics.timer(name[, meta])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the timer metric.
* `meta` {Object} Optional metadata to attach to all reports.
* Returns: {metrics.TimerFactory}

Creates a timer factory for measuring durations.

```mjs
import { timer } from 'node:metrics';

const dbQueryTimer = timer('db.query.duration');

const t = dbQueryTimer.create({ query: 'SELECT * FROM users' });
// Perform database query...
const duration = t.stop(); // Returns duration in milliseconds
```

### `metrics.uniqueSet(name[, meta])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the unique set metric.
* `meta` {Object} Optional metadata to attach to all reports.
* Returns: {metrics.UniqueSet}

Creates a unique set metric which counts distinct objects. Uniqueness is
determined through a `WeakSet`, so it follows the same identity rules.

```mjs
import { uniqueSet } from 'node:metrics';

const uniqueUsers = uniqueSet('users.unique');

uniqueUsers.add(user);        // Only counted once per unique value
uniqueUsers.add(anotherUser);
```

### `metrics.periodicGauge(name, interval, fn[, meta])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the periodic gauge metric.
* `interval` {number} The interval in milliseconds between samples.
* `fn` {Function} A function that returns the current value.
* `meta` {Object} Optional metadata to attach to all reports.
* Returns: {metrics.PeriodicGauge}

Creates a gauge that automatically samples a value at regular intervals.

```mjs
import { periodicGauge } from 'node:metrics';
import { cpuUsage } from 'node:process';

const cpu = periodicGauge('cpu.usage', 5000, () => {
  return cpuUsage().user;
});

// Stop sampling when no longer needed
cpu.stop();
```

## Classes

### Class: `MetricReport`

<!-- YAML
added: REPLACEME
-->

Represents a single metric measurement.

#### `metricReport.type`

<!-- YAML
added: REPLACEME
-->

* {string}

The type of the metric (e.g., 'counter', 'gauge', 'meter', 'periodicGauge',
'timer', 'uniqueSet').

#### `metricReport.name`

<!-- YAML
added: REPLACEME
-->

* {string}

The name of the metric.

#### `metricReport.value`

<!-- YAML
added: REPLACEME
-->

* {number}

The numeric value of the measurement.

#### `metricReport.meta`

<!-- YAML
added: REPLACEME
-->

* {Object}

Additional metadata associated with the measurement.

#### `metricReport.time`

<!-- YAML
added: REPLACEME
-->

* {number}

The `performance.now()` timestamp when the measurement was recorded in
milliseconds since `performance.timeOrigin`.

#### `metricReport.toStatsd()`

<!-- YAML
added: REPLACEME
-->

* Returns: {string}

Formats the metric report as a StatsD-compatible string.

```js
console.log(report.toStatsd()); // 'api.calls:1|c'
```

#### `metricReport.toDogStatsd()`

<!-- YAML
added: REPLACEME
-->

* Returns: {string}

Formats the metric report as a DogStatsD-compatible string with tags.

```js
console.log(report.toDogStatsd()); // 'api.calls:1|c|service:web'
```

#### `metricReport.toGraphite()`

<!-- YAML
added: REPLACEME
-->

* Returns: {string}

Formats the metric report as a Graphite-compatible string.

```js
console.log(report.toGraphite()); // 'api.calls 1 1234567890'
```

#### `metricReport.toPrometheus()`

<!-- YAML
added: REPLACEME
-->

* Returns: {string}

Formats the metric report as a Prometheus-compatible string.

```js
console.log(report.toPrometheus()); // 'api_calls{service="web"} 1 1234567890.123'
```

### Class: `Metric`

<!-- YAML
added: REPLACEME
-->

Manages the lifecycle of a metric channel and provides methods for reporting
values to it. Each metric type holds a `Metric` instance which it reports to.

#### `metric.type`

<!-- YAML
added: REPLACEME
-->

* {string}

The type of the metric (e.g., 'counter', 'gauge', 'meter', 'periodicGauge',
'timer', 'uniqueSet').

#### `metric.name`

<!-- YAML
added: REPLACEME
-->

* {string}

The name of the metric.

#### `metric.meta`

<!-- YAML
added: REPLACEME
-->

* {Object}

Additional metadata associated with the metric.

#### `metric.channelName`

<!-- YAML
added: REPLACEME
-->

* {string}

The name of the diagnostics_channel used for this metric.

#### `metric.channel`

<!-- YAML
added: REPLACEME
-->

* {Channel}

The diagnostics channel instance used for this metric.

#### `metric.shouldReport`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Indicates whether the metric should report values. This can be used to
conditionally enable or disable value preparation work.

#### `metric.report(value[, meta])`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to report.
* `meta` {Object} Additional metadata for this report.
* Returns: {metrics.MetricReport}

Reports a value for the metric, creating a `MetricReport` instance.
This bypasses the metric type specific methods, allowing direct reporting
to a channel.

Generally this method should not be used directly. Instead, use the
specific methods provided by each metric type (e.g., `increment`, `reset`,
`mark`, etc.) which internally call this method with the appropriate value and
metadata.

```mjs
import { meter } from 'node:metrics';

const apiCalls = meter('api.calls', { service: 'web' });

apiCalls.metric.report(1); // Reports a value of 1
apiCalls.metric.report(5, { endpoint: '/api/users' }); // Reports 5 with metadata
```

### Class: `Counter`

* Extends: {metrics.Gauge}

<!-- YAML
added: REPLACEME
-->

A metric that only increases or decreases.

#### `counter.metric`

<!-- YAML
added: REPLACEME
-->

* {metrics.Metric}

The underlying metric instance used for reporting.

#### `counter.increment([n[, meta]])`

<!-- YAML
added: REPLACEME
-->

* `n` {number} The amount to increment. **Default:** `1`
* `meta` {Object} Additional metadata for this report.

Increments the counter by the specified amount.

```mjs
import { counter } from 'node:metrics';

const apiCalls = counter('api.calls', { service: 'web' });

apiCalls.increment();     // Increment by 1
apiCalls.increment(5);    // Increment by 5
apiCalls.increment(10, { endpoint: '/api/users' }); // Increment by 10 with metadata
apiCalls.increment({ endpoint: '/api/orders' });    // Increment by 1 with metadata
```

#### `counter.decrement([n[, meta]])`

<!-- YAML
added: REPLACEME
-->

* `n` {number} The amount to decrement. **Default:** `1`
* `meta` {Object} Additional metadata for this report.

Decrements the counter by the specified amount.

```mjs
import { counter } from 'node:metrics';

const errorCount = counter('errors.total', { component: 'database' });

errorCount.decrement();     // Decrement by 1
errorCount.decrement(3);    // Decrement by 3
errorCount.decrement(2, { errorType: 'timeout' }); // Decrement by 2 with metadata
errorCount.decrement({ errorType: 'timeout' });    // Decrement by 1 with metadata
```

#### `counter.value`

<!-- YAML
added: REPLACEME
-->

* {number}

The current value of the counter.

### Class: `Gauge`

<!-- YAML
added: REPLACEME
-->

A metric representing a single value that can go up or down.

#### `gauge.metric`

<!-- YAML
added: REPLACEME
-->

* {metrics.Metric}

The underlying metric instance used for reporting.

#### `gauge.value`

<!-- YAML
added: REPLACEME
-->

* {number}

The current value of the metric.

#### `gauge.reset([value[, meta]])`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The new value. **Default:** `0`
* `meta` {Object} Additional metadata for this report.

Sets the gauge to a specific value and reports it.

```mjs
import { gauge } from 'node:metrics';
import { memoryUsage } from 'node:process';

const memory = gauge('memory.usage.bytes');

memory.reset(); // Reset to 0
memory.reset(memoryUsage().heapUsed); // Set to current memory usage
memory.reset(1024, { source: 'system' }); // Set to 1024 with metadata
```

#### `gauge.applyDelta(delta[, meta])`

<!-- YAML
added: REPLACEME
-->

* `delta` {number} The amount to add to the current value.
* `meta` {Object} Additional metadata for this report.

Adds a delta to the current value and reports the new value.

```mjs
import { gauge } from 'node:metrics';

const cpuUsage = gauge('cpu.usage.percent');

cpuUsage.applyDelta(5); // Increase by 5
cpuUsage.applyDelta(-2, { source: 'system' }); // Decrease by 2 with metadata
```

### Class: `Meter`

* Extends: {metrics.Gauge}

<!-- YAML
added: REPLACEME
-->

A metric that measures the rate of events over a sliding time window.

#### `meter.metric`

<!-- YAML
added: REPLACEME
-->

* {metrics.Metric}

The underlying metric instance used for reporting.

#### `meter.mark([n[, meta]])`

<!-- YAML
added: REPLACEME
-->

* `n` {number} The number of events to mark. **Default:** `1`
* `meta` {Object} Additional metadata for this report.

Records events and updates the rate calculation.

```mjs
import { meter } from 'node:metrics';

const requestRate = meter('requests.rate', 60000); // 1 minute window

requestRate.mark();     // Mark one event
requestRate.mark(10);   // Mark 10 events
requestRate.mark(5, { endpoint: '/api/users' }); // Mark 5 with metadata
requestRate.mark({ endpoint: '/api/orders' }); // Mark 1 with metadata
```

### Class: `Timer`

* Extends: {metrics.Gauge}

<!-- YAML
added: REPLACEME
-->

A metric for measuring durations.

#### `timer.metric`

<!-- YAML
added: REPLACEME
-->

* {metrics.Metric}

The underlying metric instance used for reporting.

#### `timer.start`

<!-- YAML
added: REPLACEME
-->

* {number}

The start time of the timer (milliseconds since epoch).

#### `timer.end`

<!-- YAML
added: REPLACEME
-->

* {number}

The end time of the timer (milliseconds since epoch). Zero if timer is running.

#### `timer.duration`

<!-- YAML
added: REPLACEME
-->

* {number}

The duration in milliseconds. Zero if timer is still running.

#### `timer.stop([meta])`

<!-- YAML
added: REPLACEME
-->

* `meta` {Object} Additional metadata for this report.
* Returns: {number} The duration in milliseconds.

Stops the timer and reports the duration. Can only be called once.

```mjs
import { timer } from 'node:metrics';

const dbQueryTimer = timer('db.query.duration');

const t = dbQueryTimer.create({ query: 'SELECT * FROM users' });

// Perform database query...

// Stop the timer and get the duration
const duration = t.stop(); // Returns duration in milliseconds
```

#### `timer[Symbol.dispose]()`

<!-- YAML
added: REPLACEME
-->

Allows `using` syntax to automatically stop the timer when done.

```mjs
import { timer } from 'node:metrics';

const dbQueryTimer = timer('db.query.duration');

{
  using t = dbQueryTimer.create({ query: 'SELECT * FROM users' });
  // Perform database query...

  // Timer is automatically stopped here
}
```

### Class: `TimerFactory`

<!-- YAML
added: REPLACEME
-->

A factory for creating timer instances.

#### `timer.metric`

<!-- YAML
added: REPLACEME
-->

* {metrics.Metric}

The underlying metric instance used for reporting.

#### `timerFactory.create([meta])`

<!-- YAML
added: REPLACEME
-->

* `meta` {Object} Additional metadata for this timer.
* Returns: {metrics.Timer}

Creates a new timer instance with the specified metadata.

```mjs
import { timer } from 'node:metrics';

const dbQueryTimer = timer('db.query.duration');

const t = dbQueryTimer.create({ query: 'SELECT * FROM users' });
```

### Class: `UniqueSet`

* Extends: {metrics.Gauge}

<!-- YAML
added: REPLACEME
-->

A metric that counts unique values.

#### `uniqueSet.metric`

<!-- YAML
added: REPLACEME
-->

* {metrics.Metric}

The underlying metric instance used for reporting.

#### `uniqueSet.add(value[, meta])`

<!-- YAML
added: REPLACEME
-->

* `value` {any} The value to add to the set.
* `meta` {Object} Additional metadata for this report.

Adds a value to the set. Only reports if the value hasn't been seen before.

```mjs
import { uniqueSet } from 'node:metrics';

const uniqueUsers = uniqueSet('users.unique');

uniqueUsers.add(user);        // Only counted once per unique value
uniqueUsers.add(user);        // Ignored because user is already in the set
uniqueUsers.add(anotherUser, { source: 'login' }); // Count another user with metadata
```

#### `uniqueSet.count`

<!-- YAML
added: REPLACEME
-->

* {number}

The number of unique values seen.

### Class: `PeriodicGauge`

* Extends: {metrics.Gauge}

<!-- YAML
added: REPLACEME
-->

A gauge that automatically samples values at regular intervals.

#### `periodicGauge.metric`

<!-- YAML
added: REPLACEME
-->

* {metrics.Metric}

The underlying metric instance used for reporting.

#### `periodicGauge.interval`

<!-- YAML
added: REPLACEME
-->

* {number}

The sampling interval in milliseconds. Setting this property reschedules the timer.

#### `periodicGauge.schedule()`

<!-- YAML
added: REPLACEME
-->

Schedules the periodic sampling based on the configured interval. This is called
automatically when the gauge is created, but can be called again to reschedule
after it has been stopped.

```mjs
import { periodicGauge } from 'node:metrics';
import { cpuUsage } from 'node:process';

const cpu = periodicGauge('cpu.usage', 5000, () => {
  return cpuUsage().user;
});

cpu.stop();

// Reschedule sampling
cpu.schedule();
```

#### `periodicGauge.stop()`

<!-- YAML
added: REPLACEME
-->

Stops the periodic sampling.

```mjs
import { periodicGauge } from 'node:metrics';
import { cpuUsage } from 'node:process';

const cpu = periodicGauge('cpu.usage', 5000, () => {
  return cpuUsage().user;
});

// Stop sampling when no longer needed
cpu.stop();
```

#### `periodicGauge[Symbol.dispose]()`

<!-- YAML
added: REPLACEME
-->

Allows `using` syntax to automatically stop the periodic gauge when done.

```mjs
import { periodicGauge } from 'node:metrics';
import { cpuUsage } from 'node:process';

{
  using cpu = periodicGauge('cpu.usage', 1000, () => {
    return cpuUsage().user;
  });

  // Perform operations that require periodic sampling...

  // Sampling is automatically stopped here
}
```

## Metric Streams

### `metrics.statsdStream()`

<!-- YAML
added: REPLACEME
-->

* Returns: {stream.Transform}

Creates a transform stream that converts all metrics to StatsD format.

```mjs
import { statsdStream } from 'node:metrics';
import { stdout } from 'node:process';

const stream = statsdStream();
stream.pipe(stdout);
```

Output format: `metric.name:value|type`

### `metrics.dogstatsdStream()`

<!-- YAML
added: REPLACEME
-->

* Returns: {stream.Transform}

Creates a transform stream that converts all metrics to DogStatsD format with tags.

```mjs
import { dogstatsdStream } from 'node:metrics';
import { stdout } from 'node:process';

const stream = dogstatsdStream();
stream.pipe(stdout);
```

Output format: `metric.name:value|type|key:value,key2:value2`

### `metrics.graphiteStream()`

<!-- YAML
added: REPLACEME
-->

* Returns: {stream.Transform}

Creates a transform stream that converts all metrics to Graphite plaintext protocol.

```mjs
import { dogstatsdStream } from 'node:metrics';
import { stdout } from 'node:process';

const stream = graphiteStream();
stream.pipe(stdout);
```

Output format: `metric.name value timestamp`

### `metrics.prometheusStream()`

<!-- YAML
added: REPLACEME
-->

* Returns: {stream.Transform}

Creates a transform stream that converts all metrics to Prometheus exposition format.

```mjs
import { prometheusStream } from 'node:metrics';
import { stdout } from 'node:process';

const stream = prometheusStream();
stream.pipe(stdout);
```

Output format: `metric_name{label="value"} value timestamp`

## Integration with Diagnostics Channel

All metrics publish their reports through `node:diagnostics_channel`. The channel
name format is `metrics:{type}:{name}` where `{type}` is the metric type and
`{name}` is the metric name.

```mjs
import { subscribe } from 'node:diagnostics_channel';

// Subscribe to a specific metric
subscribe('metrics:counter:api.calls', (report) => {
  console.log(`API calls: ${report.value}`);
});
```

```cjs
const { subscribe } = require('node:diagnostics_channel');

subscribe('metrics:counter:api.calls', (report) => {
  console.log(`API calls: ${report.value}`);
});
```

Additionally there is a specialized channel `metrics:new` which publishes any
newly created metrics, allowing subcribing to all metrics without needing to
know their names in advance.

```mjs
import { subscribe } from 'node:diagnostics_channel';

subscribe('metrics:new', (metric) => {
  console.log(`New metric created: ${metric.type} - ${metric.name}`);
});
```

```cjs
const { subscribe } = require('node:diagnostics_channel');

subscribe('metrics:new', (metric) => {
  console.log(`New metric created: ${metric.type} - ${metric.name}`);
});
```

## Best Practices

1. **Naming Conventions**: Use dot-separated hierarchical names (e.g., `http.requests.total`).

2. **Metadata**: Use metadata to add dimensions to your metrics without creating separate metric instances.

3. **Performance**: Metric types are designed to be lightweight. However, avoid
  creating metric types in hot code paths. As with diagnostics_channel, metric
  creation is optimized for capture time performance by moving costly
  operations to metric type creation time.
