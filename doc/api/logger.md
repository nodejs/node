# Logger

<!--introduced_in=v26.0.0-->

> Stability: 1 - Experimental

<!-- source_link=lib/logger.js -->

The `node:logger` module provides structured logging capabilities for Node.js
applications.

## Class: `LogConsumer`

### `consumer.enabled(level)`

<!-- YAML
added: REPLACEME
-->

* `level` {string} The log level to check (e.g., `'debug'`, `'info'`, `'warn'`,
  `'error'`, `'fatal'`).
* Returns: {boolean} `true` if the level is enabled, `false` otherwise.

Checks if a specific log level is enabled for this consumer.

This method returns `false` for unknown log levels without throwing an error.
Log levels are case-sensitive and must be one of the predefined levels:
`'trace'`, `'debug'`, `'info'`, `'warn'`, `'error'`, `'fatal'`.

```js
const { LogConsumer } = require('node:logger');

const consumer = new LogConsumer({ level: 'info' });

console.log(consumer.enabled('debug')); // false (below threshold)
console.log(consumer.enabled('info'));  // true
console.log(consumer.enabled('error')); // true
console.log(consumer.enabled('DEBUG')); // false (unknown level - case sensitive)
console.log(consumer.enabled('unknown')); // false (unknown level)
```
