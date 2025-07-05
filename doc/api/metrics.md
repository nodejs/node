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
import { counter } from 'node:metrics';

// Create counter metrics
const apiCalls = counter('api.calls', { service: 'web' });
const requestDuration = counter('api.request.duration.ms', { service: 'web' });

// Use metrics in your application
function handleRequest(req, res) {
  const timer = requestDuration.createTimer({ endpoint: req.url });

  apiCalls.increment();

  // Process request...

  timer.stop(); // Increments requestDuration with the elapsed time
}
```

```cjs
const { counter } = require('node:metrics');

// Create counter metrics
const apiCalls = counter('api.calls', { service: 'web' });
const requestDuration = counter('api.request.duration.ms', { service: 'web' });

// Use metrics in your application
function handleRequest(req, res) {
  const timer = requestDuration.createTimer({ endpoint: req.url });

  apiCalls.increment();

  // Process request...

  timer.stop(); // Increments requestDuration with the elapsed time
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
```


### `metrics.pullGauge(name, fn[, meta])`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the pull gauge metric.
* `fn` {Function} A function that returns the current value.
* `meta` {Object} Optional metadata to attach to all reports.
* Returns: {metrics.PullGauge}

Creates a gauge that samples a value on-demand by calling the provided function.

```mjs
import { pullGauge } from 'node:metrics';
import { cpuUsage } from 'node:process';

const cpu = pullGauge('cpu.usage', () => {
  return cpuUsage().user;
});

// Sample the gauge when needed
cpu.sample();
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

The type of the metric (e.g., 'counter', 'gauge', 'pullGauge',
'timer').

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

The type of the metric (e.g., 'counter', 'gauge', 'pullGauge',
'timer').

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
import { gauge } from 'node:metrics';

const memoryUsage = gauge('memory.usage', { service: 'web' });

memoryUsage.report(85); // Reports a value of 85
memoryUsage.report(90, { threshold: 'warning' }); // Reports 90 with metadata
```

### Class: `Counter`

* Extends: {metrics.Metric}

<!-- YAML
added: REPLACEME
-->

A metric that only increases or decreases.

#### `counter.value`

<!-- YAML
added: REPLACEME
-->

* {number}

The current value of the counter.

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

#### `counter.createTimer([meta])`

<!-- YAML
added: REPLACEME
-->

* `meta` {Object} Additional metadata to include with the report.
* Returns: {Timer}

Creates a timer that will increment this counter with its duration when stopped.

```mjs
import { counter } from 'node:metrics';

const requestDuration = counter('request.duration.ms');

const timer = requestDuration.createTimer({ endpoint: '/api/users' });
// Process request...
const duration = timer.stop(); // Counter is incremented with duration
```

### Class: `Gauge`

* Extends: {metrics.Metric}

<!-- YAML
added: REPLACEME
-->

A metric representing a single value that can go up or down.

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

#### `gauge.createTimer([meta])`

<!-- YAML
added: REPLACEME
-->

* `meta` {Object} Additional metadata to include with the report.
* Returns: {Timer}

Creates a timer that will set this gauge to its duration when stopped.

```mjs
import { gauge } from 'node:metrics';

const responseTime = gauge('response.time.ms');

const timer = responseTime.createTimer({ endpoint: '/api/users' });
// Process request...
const duration = timer.stop(); // Gauge is set to duration
```

### Class: `Timer`

<!-- YAML
added: REPLACEME
-->

A helper for measuring durations that reports the elapsed time via a callback when stopped.

#### `timer.start`

<!-- YAML
added: REPLACEME
-->

* {number}

The start time of the timer (milliseconds since `performance.timeOrigin`). This property is read-only.

#### `timer.end`

<!-- YAML
added: REPLACEME
-->

* {number|undefined}

The end time of the timer (milliseconds since `performance.timeOrigin`). `undefined` if timer is still running. This property is read-only.

#### `timer.duration`

<!-- YAML
added: REPLACEME
-->

* {number|undefined}

The duration in milliseconds. `undefined` if timer is still running. This property is read-only.

#### `timer.stop()`

<!-- YAML
added: REPLACEME
-->

* Returns: {number} The duration in milliseconds.

Stops the timer and reports the duration. Can only be called once.

```mjs
import { counter } from 'node:metrics';

const dbQueryDuration = counter('db.query.duration');

const t = dbQueryDuration.createTimer({ query: 'SELECT * FROM users' });

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
import { counter } from 'node:metrics';

const dbQueryDuration = counter('db.query.duration');

{
  using t = dbQueryDuration.createTimer({ query: 'SELECT * FROM users' });
  // Perform database query...

  // Timer is automatically stopped here
}
```


### Class: `PullGauge`

* Extends: {metrics.Metric}

<!-- YAML
added: REPLACEME
-->

A gauge that samples values on-demand when the `sample()` method is called.

#### `pullGauge.sample([meta])`

<!-- YAML
added: REPLACEME
-->

* `meta` {Object} Additional metadata for this specific sample.
* Returns: {number} The sampled value.

Calls the configured function to get the current value and reports it.

```mjs
import { pullGauge } from 'node:metrics';
import { cpuUsage } from 'node:process';

const cpu = pullGauge('cpu.usage', () => {
  return cpuUsage().user;
});

// Sample the gauge when needed
const value = cpu.sample();
console.log(`Current CPU usage: ${value}`);

// Sample with additional metadata
cpu.sample({ threshold: 'high' });
```


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
