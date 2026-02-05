# Logger

<!--introduced_in=v26.0.0-->

> Stability: 1.0 - Early Development

<!-- source_link=lib/logger.js -->

The `node:logger` module provides high-performance structured logging
capabilities for Node.js applications. It uses `diagnostics_channel` internally
to dispatch log events to consumers, allowing multiple consumers to receive
logs independently.

```mjs
import { Logger, JSONConsumer } from 'node:logger';

const logger = new Logger({ level: 'info' });
const consumer = new JSONConsumer({ level: 'info' });
consumer.attach();

logger.info('Hello world');
// Outputs: {"level":"info","time":1234567890,"msg":"Hello world"}
```

```cjs
const { Logger, JSONConsumer } = require('node:logger');

const logger = new Logger({ level: 'info' });
const consumer = new JSONConsumer({ level: 'info' });
consumer.attach();

logger.info('Hello world');
// Outputs: {"level":"info","time":1234567890,"msg":"Hello world"}
```

## Log levels

The logger supports the following log levels, in order of severity:

| Level   | Value | Description                    |
| ------- | ----- | ------------------------------ |
| `trace` | 10    | Detailed debugging information |
| `debug` | 20    | Debug information              |
| `info`  | 30    | General information            |
| `warn`  | 40    | Warning messages               |
| `error` | 50    | Error messages                 |
| `fatal` | 60    | Critical errors                |

Log levels follow [RFC 5424][] numerical ordering.

## Class: `Logger`

<!-- YAML
added: REPLACEME
-->

The `Logger` class is used to create log records and publish them to
`diagnostics_channel` channels.

### `new Logger([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `level` {string} Minimum log level. **Default:** `'info'`.
  * `bindings` {Object} Context fields added to all log records.
    **Default:** `{}`.
  * `serializers` {Object} Custom serializer functions for specific fields.
    **Default:** `{}`.

Creates a new `Logger` instance.

```mjs
import { Logger } from 'node:logger';

const logger = new Logger({
  level: 'debug',
  bindings: { service: 'my-app', version: '1.0.0' },
});
```

```cjs
const { Logger } = require('node:logger');

const logger = new Logger({
  level: 'debug',
  bindings: { service: 'my-app', version: '1.0.0' },
});
```

### `logger.trace(msg[, fields])`

### `logger.trace(obj)`

### `logger.trace(error[, fields])`

<!-- YAML
added: REPLACEME
-->

* `msg` {string} Log message.
* `obj` {Object} Object containing `msg` property and additional fields.
* `error` {Error} Error object to log.
* `fields` {Object} Additional fields to include in the log record.

Logs a message at the `trace` level.

```js
logger.trace('Detailed trace message');
logger.trace('User action', { userId: 123, action: 'click' });
logger.trace({ msg: 'Object format', requestId: 'abc123' });
```

### `logger.debug(msg[, fields])`

### `logger.debug(obj)`

### `logger.debug(error[, fields])`

<!-- YAML
added: REPLACEME
-->

* `msg` {string} Log message.
* `obj` {Object} Object containing `msg` property and additional fields.
* `error` {Error} Error object to log.
* `fields` {Object} Additional fields to include in the log record.

Logs a message at the `debug` level.

```js
logger.debug('Debug information');
logger.debug('Processing request', { requestId: 'abc123' });
```

### `logger.info(msg[, fields])`

### `logger.info(obj)`

### `logger.info(error[, fields])`

<!-- YAML
added: REPLACEME
-->

* `msg` {string} Log message.
* `obj` {Object} Object containing `msg` property and additional fields.
* `error` {Error} Error object to log.
* `fields` {Object} Additional fields to include in the log record.

Logs a message at the `info` level.

```js
logger.info('Server started');
logger.info('Request received', { method: 'GET', path: '/api/users' });
logger.info({ msg: 'User logged in', userId: 123 });
```

### `logger.warn(msg[, fields])`

### `logger.warn(obj)`

### `logger.warn(error[, fields])`

<!-- YAML
added: REPLACEME
-->

* `msg` {string} Log message.
* `obj` {Object} Object containing `msg` property and additional fields.
* `error` {Error} Error object to log.
* `fields` {Object} Additional fields to include in the log record.

Logs a message at the `warn` level.

```js
logger.warn('Deprecated API used');
logger.warn('High memory usage', { memoryUsage: process.memoryUsage() });
```

### `logger.error(msg[, fields])`

### `logger.error(obj)`

### `logger.error(error[, fields])`

<!-- YAML
added: REPLACEME
-->

* `msg` {string} Log message.
* `obj` {Object} Object containing `msg` property and additional fields.
* `error` {Error} Error object to log.
* `fields` {Object} Additional fields to include in the log record.

Logs a message at the `error` level.

```js
logger.error('Database connection failed');
logger.error(new Error('Something went wrong'));
logger.error(new Error('Request failed'), { requestId: 'abc123' });
```

### `logger.fatal(msg[, fields])`

### `logger.fatal(obj)`

### `logger.fatal(error[, fields])`

<!-- YAML
added: REPLACEME
-->

* `msg` {string} Log message.
* `obj` {Object} Object containing `msg` property and additional fields.
* `error` {Error} Error object to log.
* `fields` {Object} Additional fields to include in the log record.

Logs a message at the `fatal` level.

```js
logger.fatal('Application crash');
logger.fatal(new Error('Unrecoverable error'));
```

### `logger.child(bindings[, options])`

<!-- YAML
added: REPLACEME
-->

* `bindings` {Object} Additional context fields for the child logger.
* `options` {Object}
  * `level` {string} Log level for the child logger.
  * `serializers` {Object} Additional serializers for the child logger.
* Returns: {Logger} A new child logger instance.

Creates a child logger with additional context bindings. Child loggers inherit
the parent's configuration and add their own bindings to all log records.

> **Note for library authors:** The `level` option in `child()` is intended for
> application code only. Library and module authors should NOT override the log
> level in child loggers. Instead, libraries should inherit the parent logger's
> level to respect the application developer's log level configuration.
> Application developers can use this feature to isolate specific components
> or adjust verbosity for particular subsystems they directly control.

```mjs
import { Logger } from 'node:logger';

const logger = new Logger({ bindings: { service: 'my-app' } });
const requestLogger = logger.child({ requestId: 'abc123' });

requestLogger.info('Processing request');
// Log includes: service: 'my-app', requestId: 'abc123'
```

```cjs
const { Logger } = require('node:logger');

const logger = new Logger({ bindings: { service: 'my-app' } });
const requestLogger = logger.child({ requestId: 'abc123' });

requestLogger.info('Processing request');
// Log includes: service: 'my-app', requestId: 'abc123'
```

### `logger.<level>.enabled`

<!-- YAML
added: REPLACEME
-->

* {boolean} `true` if the level is enabled, `false` otherwise.

Each log method (`trace`, `debug`, `info`, `warn`, `error`, `fatal`) has an
`enabled` property that indicates whether that level is enabled for this logger.

Use this to check if a level is enabled before performing expensive computations:

```js
if (logger.debug.enabled) {
  // Perform expensive debug computation only if debug is enabled
  logger.debug('Debug info', { expensiveData: computeDebugData() });
}

// Typos will throw a TypeError (safer than silent failure)
// logger.debg.enabled → TypeError: Cannot read properties of undefined
```

For dynamic level checks, use property access:

```js
const level = config.logLevel; // 'info', 'debug', etc.
if (logger[level]?.enabled) {
  logger[level]('Dynamic log message');
}
```

## Class: `LogConsumer`

<!-- YAML
added: REPLACEME
-->

The `LogConsumer` class is the base class for log consumers. Consumers
subscribe to `diagnostics_channel` events and process log records.

### `new LogConsumer([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `level` {string} Minimum log level to consume. **Default:** `'info'`.

Creates a new `LogConsumer` instance.

### `consumer.attach()`

<!-- YAML
added: REPLACEME
-->

Attaches the consumer to log channels. After calling this method, the consumer
will receive log records from all loggers.

```js
const consumer = new JSONConsumer({ level: 'info' });
consumer.attach();
// Consumer now receives all log records at 'info' level and above
```

### `consumer.<level>.enabled`

<!-- YAML
added: REPLACEME
-->

* {boolean} `true` if the level is enabled, `false` otherwise.

Each log level (`trace`, `debug`, `info`, `warn`, `error`, `fatal`) has an
`enabled` property that indicates whether that level is enabled for this
consumer.

```js
const { LogConsumer } = require('node:logger');

const consumer = new LogConsumer({ level: 'info' });

console.log(consumer.debug.enabled); // false (below threshold)
console.log(consumer.info.enabled);  // true
console.log(consumer.error.enabled); // true

// Typos will throw a TypeError (safer than silent failure)
// consumer.debg.enabled → TypeError: Cannot read properties of undefined
```

### `consumer.handle(record)`

<!-- YAML
added: REPLACEME
-->

* `record` {Object} The log record to handle.
  * `level` {string} Log level.
  * `msg` {string} Log message.
  * `time` {number} Timestamp in milliseconds.
  * `bindingsStr` {string} Pre-serialized bindings JSON string.
  * `fields` {Object} Additional log fields.

Handles a log record. Subclasses must implement this method.

```mjs
import { LogConsumer } from 'node:logger';

class CustomConsumer extends LogConsumer {
  handle(record) {
    console.log(`[${record.level}] ${record.msg}`);
  }
}
```

```cjs
const { LogConsumer } = require('node:logger');

class CustomConsumer extends LogConsumer {
  handle(record) {
    console.log(`[${record.level}] ${record.msg}`);
  }
}
```

## Class: `JSONConsumer`

<!-- YAML
added: REPLACEME
-->

* Extends: {LogConsumer}

The `JSONConsumer` class outputs log records as JSON to a stream.

### `new JSONConsumer([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `level` {string} Minimum log level to consume. **Default:** `'info'`.
  * `stream` {number|string|Object} Output destination. Can be a file
    descriptor (number), file path (string), or a writable stream object.
    **Default:** `stdout` (fd 1).
  * `fields` {Object} Additional fields to include in every log record.
    **Default:** `{}`.

Creates a new `JSONConsumer` instance.

```mjs
import { JSONConsumer } from 'node:logger';

// Output to stdout (default)
const consumer1 = new JSONConsumer({ level: 'info' });

// Output to a file
const consumer2 = new JSONConsumer({
  level: 'debug',
  stream: '/var/log/app.log',
});

// Output to stderr
const consumer3 = new JSONConsumer({
  level: 'error',
  stream: 2,
});

// Add fields to every log
const consumer4 = new JSONConsumer({
  level: 'info',
  fields: { hostname: 'server-1', env: 'production' },
});
```

```cjs
const { JSONConsumer } = require('node:logger');

// Output to stdout (default)
const consumer1 = new JSONConsumer({ level: 'info' });

// Output to a file
const consumer2 = new JSONConsumer({
  level: 'debug',
  stream: '/var/log/app.log',
});
```

### `consumer.flush([callback])`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function} Called when flush completes.

Flushes pending writes to the underlying stream.

```js
consumer.flush(() => {
  console.log('All logs flushed');
});
```

### `consumer.flushSync()`

<!-- YAML
added: REPLACEME
-->

Flushes pending writes synchronously.

```js
consumer.flushSync();
```

### `consumer.end()`

<!-- YAML
added: REPLACEME
-->

Closes the consumer and its underlying stream.

```js
consumer.end();
```

## `logger.stdSerializers`

<!-- YAML
added: REPLACEME
-->

* {Object}

An object containing standard serializer functions for common objects.

### `stdSerializers.err(error)`

<!-- YAML
added: REPLACEME
-->

* `error` {Error} Error object to serialize.
* Returns: {Object} Serialized error object.

Serializes an `Error` object for logging. Includes `type`, `message`, `stack`,
and any additional properties. Recursively serializes `cause` if present.

```mjs
import { Logger, stdSerializers } from 'node:logger';

const logger = new Logger({
  serializers: {
    err: stdSerializers.err,
  },
});
```

```cjs
const { Logger, stdSerializers } = require('node:logger');

const logger = new Logger({
  serializers: {
    err: stdSerializers.err,
  },
});
```

### `stdSerializers.req(request)`

<!-- YAML
added: REPLACEME
-->

* `request` {http.IncomingMessage} HTTP request object.
* Returns: {Object} Serialized request object with `method`, `url`, `headers`,
  `remoteAddress`, and `remotePort`.

Serializes an HTTP request object for logging.

```js
const http = require('node:http');
const { Logger, JSONConsumer, stdSerializers } = require('node:logger');

const logger = new Logger({
  serializers: {
    req: stdSerializers.req,
  },
});
const consumer = new JSONConsumer();
consumer.attach();

http.createServer((req, res) => {
  logger.info('Request received', { req });
  res.end('OK');
}).listen(3000);
```

### `stdSerializers.res(response)`

<!-- YAML
added: REPLACEME
-->

* `response` {http.ServerResponse} HTTP response object.
* Returns: {Object} Serialized response object with `statusCode` and `headers`.

Serializes an HTTP response object for logging.

```js
logger.info('Response sent', { res });
```

## `logger.LEVELS`

<!-- YAML
added: REPLACEME
-->

* {Object}

An object mapping log level names to their numeric values.

```js
const { LEVELS } = require('node:logger');

console.log(LEVELS);
// { trace: 10, debug: 20, info: 30, warn: 40, error: 50, fatal: 60 }
```

## `logger.channels`

<!-- YAML
added: REPLACEME
-->

* {Object}

An object containing `diagnostics_channel` instances for each log level.
Advanced users can subscribe directly to these channels.

```js
const { channels } = require('node:logger');

channels.info.subscribe((record) => {
  // Custom handling of info-level logs
});
```

## `logger.serialize`

<!-- YAML
added: REPLACEME
-->

* {symbol}

A symbol that objects can implement to define custom serialization behavior
for logging. Similar to [`util.inspect.custom`][].

When an object with a `[serialize]()` method is logged, the logger will call
that method instead of serializing the object directly. This allows objects
to control which properties are included in logs, filtering out sensitive
data like passwords or tokens.

```mjs
import { Logger, JSONConsumer, serialize } from 'node:logger';

class User {
  constructor(id, name, password) {
    this.id = id;
    this.name = name;
    this.password = password; // Sensitive!
  }

  // Define custom serialization
  [serialize]() {
    return {
      id: this.id,
      name: this.name,
      // password is excluded
    };
  }
}

const consumer = new JSONConsumer();
consumer.attach();

const logger = new Logger();
const user = new User(1, 'Alice', 'secret123');

logger.info({ msg: 'User logged in', user });
// Output: {"level":"info","time":...,"msg":"User logged in","user":{"id":1,"name":"Alice"}}
// Note: password is not included in the output
```

```cjs
const { Logger, JSONConsumer, serialize } = require('node:logger');

class DatabaseConnection {
  constructor(host, user, password) {
    this.host = host;
    this.user = user;
    this.password = password;
  }

  [serialize]() {
    return {
      host: this.host,
      user: this.user,
      connected: this.isConnected,
      // password is excluded
    };
  }
}
```

The `serialize` symbol takes precedence over field-specific serializers.
If an object has both a `[serialize]()` method and a matching serializer
in the logger's `serializers` option, the `[serialize]()` method will be used.

## Examples

### Basic usage

```mjs
import { Logger, JSONConsumer } from 'node:logger';

// Create a logger
const logger = new Logger({ level: 'info' });

// Create and attach a consumer
const consumer = new JSONConsumer({ level: 'info' });
consumer.attach();

// Log messages
logger.info('Application started');
logger.info('User logged in', { userId: 123 });
logger.error(new Error('Something went wrong'));
```

### Child loggers for request tracing

```mjs
import { Logger, JSONConsumer } from 'node:logger';
import { randomUUID } from 'node:crypto';

const logger = new Logger({
  bindings: { service: 'api-server' },
});

const consumer = new JSONConsumer();
consumer.attach();

function handleRequest(req, res) {
  const requestLogger = logger.child({
    requestId: randomUUID(),
    method: req.method,
    path: req.url,
  });

  requestLogger.info('Request started');
  // ... handle request ...
  requestLogger.info('Request completed', { statusCode: res.statusCode });
}
```

### Multiple consumers

```mjs
import { Logger, JSONConsumer } from 'node:logger';

const logger = new Logger({ level: 'trace' });

// Console output for development (info and above)
const consoleConsumer = new JSONConsumer({ level: 'info' });
consoleConsumer.attach();

// File output for debugging (all levels)
const fileConsumer = new JSONConsumer({
  level: 'trace',
  stream: '/var/log/app-debug.log',
});
fileConsumer.attach();

// Error file (errors only)
const errorConsumer = new JSONConsumer({
  level: 'error',
  stream: '/var/log/app-error.log',
});
errorConsumer.attach();
```

### Custom serializers

```mjs
import { Logger, JSONConsumer, stdSerializers } from 'node:logger';

const logger = new Logger({
  serializers: {
    err: stdSerializers.err,
    req: stdSerializers.req,
    res: stdSerializers.res,
    user: (user) => ({ id: user.id, email: user.email }), // Custom serializer
  },
});

const consumer = new JSONConsumer();
consumer.attach();

logger.info('User action', {
  user: { id: 1, email: 'user@example.com', password: 'secret' },
});
// Output will not include password due to custom serializer
```

### Custom consumer

```mjs
import { LogConsumer } from 'node:logger';

class ConsoleColorConsumer extends LogConsumer {
  handle(record) {
    const colors = {
      trace: '\x1b[90m',
      debug: '\x1b[36m',
      info: '\x1b[32m',
      warn: '\x1b[33m',
      error: '\x1b[31m',
      fatal: '\x1b[35m',
    };
    const reset = '\x1b[0m';
    const color = colors[record.level] || reset;

    console.log(`${color}[${record.level.toUpperCase()}]${reset} ${record.msg}`);
  }
}

const consumer = new ConsoleColorConsumer({ level: 'debug' });
consumer.attach();
```

[RFC 5424]: https://www.rfc-editor.org/rfc/rfc5424.html
[`util.inspect.custom`]: util.md#utilinspectcustom
