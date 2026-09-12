# Logger

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1.1 - Active development

<!-- source_link=lib/logger.js -->

The `node:logger` module provides a structured logging facade. A logger composes
log events and passes them to a provider. Providers control filtering, encoding,
buffering, and output.

```mjs
import create from 'node:logger';

const logger = create({ name: 'example' });
logger.info('server started', { port: 3000 });
```

```cjs
const create = require('node:logger');

const logger = create({ name: 'example' });
logger.info('server started', { port: 3000 });
```

The module is only available under the `node:` scheme, and only when Node.js is
started with the [`--experimental-logger`][] flag.

## Log events

Providers receive a log event with the following properties:

* `timestamp` {number} Milliseconds since the Unix epoch.
* `level` {Object}
  * `name` {string} The level name.
  * `value` {number} The numeric level value. Larger values are more severe.
* `name` {string|undefined} The logger name.
* `message` {any} The log message value.
* `bindings` {Object} Attributes associated with the logger and its parents.
* `attributes` {Object} Attributes supplied for this event.

The event, level, bindings, and attributes objects are shallowly frozen. The
message value is retained as provided and is not cloned or frozen. Nested values
are not cloned or frozen. Providers must treat the entire event as read-only and
must copy nested values before retaining or modifying them asynchronously.

If a direct value of `bindings` or `attributes` is a function, the logger invokes
it for every event and replaces it in the event with the returned value. Nested
functions are not invoked.

## `logger.getDefaultProvider()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object}

Returns the provider used when a logger is created without an explicit provider.
Until set, the first call or logger creation lazily creates a
[`ConsoleProvider`][]. The same default provider is reused by subsequent loggers.

## `logger.setDefaultProvider(provider)`

<!-- YAML
added: REPLACEME
-->

* `provider` {Object} The provider to use by default.

Sets the provider used when a logger is created without an explicit provider.
Existing loggers retain their provider.

```cjs
const create = require('node:logger');

create.setDefaultProvider(new create.EventProvider());
const logger = create();
```

When `provider` differs from the current default, the process emits the
[`'defaultLoggerProviderChanged'`][] event with `provider` and the current
default provider. This event allows an application to detect when a dependency
changes the default provider. If the application does not wish to allow the
default provider to be changed, it can throw an error synchronously which will
be propagated up to the code that called `setDefaultProvider()`.

## `logger.create([provider][, options])`

<!-- YAML
added: REPLACEME
-->

* `provider` {Object} The provider that receives log events. **Default:**
  The value returned by [`logger.getDefaultProvider()`][].
* `options` {Object}
  * `name` {string} The logger name.
  * `bindings` {Object} Attributes included in every event from the logger.
    **Default:** `{}`.
* Returns: {Logger}

Creates a logger. If the first argument does not implement the provider
contract, it is treated as `options`.

```cjs
const { create, EventProvider } = require('node:logger');

const defaultLogger = create();

const provider = new EventProvider();
provider.on('log', (event) => {
  // Handle the structured event.
});
const eventLogger = create(provider, {
  name: 'api',
  bindings: { service: 'users' },
});
```

## Provider contract

A provider is an object with a synchronous `log(event, context)` method. It may also
implement `isEnabled(level, context)` to prevent events from being composed:

```cjs
class CustomProvider {
  isEnabled(level, context) {
    return level.value >= 30;
  }

  log(event, context) {
    // Handle the structured event.
  }
}
```

The `level` passed to `isEnabled()` is an immutable level descriptor. The same
immutable `context` is passed to both methods and contains the logger's `name`
and `bindings`. Only an exact `false` return value disables the event. A provider
without `isEnabled()` receives every event.

Provider methods run synchronously in the logging call. Exceptions propagate to
the caller. A provider that performs asynchronous work must enqueue or copy the
event synchronously.

## Class: `Logger`

<!-- YAML
added: REPLACEME
-->

### `new Logger([provider][, options])`

<!-- YAML
added: REPLACEME
-->

* `provider` {Object} The provider that receives log events. **Default:**
  The value returned by [`logger.getDefaultProvider()`][].
* `options` {Object}
  * `name` {string} The logger name.
  * `bindings` {Object} Attributes included in every event from the logger.

Equivalent to [`logger.create()`][].

### `logger.provider`

<!-- YAML
added: REPLACEME
-->

* {Object}

The provider used by the logger. The property is read-only.

### `logger.name`

<!-- YAML
added: REPLACEME
-->

* {string|undefined}

The logger name.

### `logger.bindings`

<!-- YAML
added: REPLACEME
-->

* {Object}

The shallowly frozen bindings associated with the logger.

### `logger.trace(message[, attributes])`

### `logger.debug(message[, attributes])`

### `logger.info(message[, attributes])`

### `logger.warn(message[, attributes])`

### `logger.error(message[, attributes])`

### `logger.fatal(message[, attributes])`

<!-- YAML
added: REPLACEME
-->

* `message` {any} The log message value.
* `attributes` {Object} Attributes associated with this event. **Default:** `{}`.

Creates a structured event at the corresponding level and passes it to the
provider if the provider enables that level.

```cjs
logger.info('request completed', {
  method: 'GET',
  statusCode: 200,
});
```

### `logger.log(level, message[, attributes])`

<!-- YAML
added: REPLACEME
-->

* `level` {string|Object} A built-in level name or an object containing a
  `name` {string} and integer `value` {number}.
* `message` {any} The log message value.
* `attributes` {Object} Attributes associated with this event. **Default:** `{}`.

Creates an event at a built-in or custom level.

```cjs
logger.log({ name: 'notice', value: 35 }, 'configuration reloaded');
```

### `logger.isEnabled(level)`

<!-- YAML
added: REPLACEME
-->

* `level` {string|Object} A built-in level name or custom level descriptor.
* Returns: {boolean}

Returns whether the provider enables the level for this logger.

### `logger.child(bindings[, options])`

<!-- YAML
added: REPLACEME
-->

* `bindings` {Object} Additional logger bindings.
* `options` {Object}
  * `name` {string} A name for the child. **Default:** The parent logger's name.
* Returns: {Logger}

Creates a logger that uses the same provider and adds to the parent's bindings.
Child bindings with the same key replace parent bindings.

## Class: `AggregateProvider`

<!-- YAML
added: REPLACEME
-->

The `AggregateProvider` delivers log events to a fixed list of providers. It
enables an event when at least one provider enables it. Each provider's
`isEnabled()` method is checked again immediately before delivery, and a
provider that returns `false` does not receive the event. Providers are called
in list order. Exceptions propagate immediately and prevent later providers
from being called.

### `new AggregateProvider(providers)`

<!-- YAML
added: REPLACEME
-->

* `providers` {Object\[]} The providers that receive log events.

The array may be empty. The providers are copied and validated during
construction. Duplicate and nested aggregate providers are supported.

### `aggregateProvider.providers`

<!-- YAML
added: REPLACEME
-->

* {Object\[]}

The frozen provider list. The provider objects themselves are not frozen.

## Class: `ConsoleProvider`

<!-- YAML
added: REPLACEME
-->

The `ConsoleProvider` serializes each event and writes it followed by a newline
using [`fs.Utf8Stream`][]. Events are serialized as JSON by default.

### `new ConsoleProvider([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `destination` {string} Either `'stdout'` or `'stderr'`. **Default:**
    `'stdout'`.
  * `flatten` {boolean} Whether to copy bindings and attributes into the
    top-level serialized record. **Default:** `false`.
  * `level` {string|Object} The minimum enabled level. **Default:** `'info'`.
  * `maxLength` {number} The maximum internal buffer length. Writes that would
    exceed this value are dropped. **Default:** `0`, for no limit.
  * `minLength` {number} The minimum internal buffer length before an automatic
    flush. **Default:** `0`.
  * `periodicFlush` {number} The interval in milliseconds at which the stream
    is flushed. **Default:** `0`, for no periodic flush.
  * `pid` {boolean} Whether to add the process ID as a top-level `pid` property
    to serialized records. **Default:** `false`.
  * `serializer` {Function} A function that receives a log record and returns
    a string. **Default:** A JSON serializer.
  * `sync` {boolean} Whether writes are synchronous. **Default:** `false`.

When `flatten` is `true`, bindings are copied into the top-level record first,
followed by attributes. Attributes therefore replace bindings with the same
property name. The level descriptor is emitted as a numeric `level` property and
a string `levelName` property. The `level`, `levelName`, `message`, `name`, and
`timestamp` event properties, along with `pid` when enabled, take precedence
over both. The original `bindings` and `attributes` containers are not included
in the record.

The default serializer follows [`JSON.stringify()`][] semantics, including
calling `toJSON()` methods, with these additions:

* `BigInt` values are serialized as decimal strings.
* `Error` objects include their `name`, `message`, and `stack`, along with
  `code`, `cause`, `errors`, and enumerable properties when present.
* Circular references are serialized as the string `'[Circular]'`.

Values that [`JSON.stringify()`][] normally omits, such as `undefined`,
functions, and symbols used as object properties, are still omitted.

[`util.inspect()`][] can be used when JavaScript-style diagnostic output is
preferred over JSON:

```cjs
const { ConsoleProvider, create } = require('node:logger');
const { inspect } = require('node:util');

const logger = create(new ConsoleProvider({ serializer: inspect }));
logger.info('started', { processId: 1n });
```

### `consoleProvider.destination`

<!-- YAML
added: REPLACEME
-->

* {string}

The configured destination.

### `consoleProvider.flatten`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Whether bindings and attributes are flattened into the serialized record.

### `consoleProvider.level`

<!-- YAML
added: REPLACEME
-->

* {Object}

The minimum level descriptor. Set this property to a built-in level name or a
custom level descriptor to update the minimum level.

### `consoleProvider.pid`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Whether serialized records include the process ID.

### `consoleProvider.serializer`

<!-- YAML
added: REPLACEME
-->

* {Function}

The configured serializer.

### `consoleProvider.stream`

<!-- YAML
added: REPLACEME
-->

* {fs.Utf8Stream}

The underlying stream. Its `'drop'` event reports records dropped because of
`maxLength`.

### `consoleProvider.flush([callback])`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function} Called when pending writes have completed.

Flushes pending writes.

### `consoleProvider.flushSync()`

<!-- YAML
added: REPLACEME
-->

Synchronously flushes pending writes. An `ERR_INVALID_STATE` error is thrown if
the provider's stream is currently writing.

## Class: `DiagnosticsProvider`

<!-- YAML
added: REPLACEME
-->

The `DiagnosticsProvider` publishes enabled log events to
[`diagnostics_channel`][] channels. Events are only enabled for channels that
have subscribers.

### `new DiagnosticsProvider(channels)`

<!-- YAML
added: REPLACEME
-->

* `channels` {Object|Function} A channel mapping or routing function.

When `channels` is an object, each own string or symbol property is a channel
name and its value is a selector function that receives a level descriptor. The
event is published to every subscribed channel whose selector returns a truthy
value.

When `channels` is a function, it receives a level descriptor and returns the
string or symbol name of the channel to publish to. Returning `undefined`
disables the event. The selected channel must have subscribers for the event to
be enabled.

`DiagnosticsProvider.isEnabled()` invokes the routing function to resolve its
channel. For a channel mapping, it invokes selectors only for channels that
currently have subscribers and stops after the first matching selector. When an
event is enabled, the applicable routing function or selectors are invoked
again immediately before publishing it. These functions must not rely on being
called exactly once.

```cjs
const { DiagnosticsProvider, create, levels } = require('node:logger');

const provider = new DiagnosticsProvider({
  'application:log': () => true,
  'application:error': (level) => level.value >= levels.error.value,
});
const logger = create(provider);
```

## Class: `EventProvider`

<!-- YAML
added: REPLACEME
-->

* Extends: {EventEmitter}

For each enabled log event, the `EventProvider` first checks for listeners whose
event name exactly matches the level name. If any exist, it emits the event using
the level name. Otherwise, it emits a `'log'` event. Event selection is based
only on the level name, not its numeric value. The provider does not buffer log
events, so events emitted without a matching or fallback listener are discarded.

### `new EventProvider([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `level` {string|Object} The minimum enabled level. **Default:** `'trace'`.

### Event: `'<level name>'`

<!-- YAML
added: REPLACEME
-->

* `event` {Object} The structured log event.

Emitted synchronously when the provider has a listener for the exact level name.
This event takes precedence over the `'log'` event.

```mjs
import { create, EventProvider } from 'node:logger';

const ep = new EventProvider();

ep.on('warn', (event) => {
  // Receives all warn logs
});

ep.on('log', (event) => {
  // Receives all other logs
});

const logger = create(ep);
logger.warn('warning!');
logger.info('info!');
```

### Event: `'log'`

<!-- YAML
added: REPLACEME
-->

* `event` {Object} The structured log event.

Emitted synchronously when the provider has no listener for the event's level
name.

### `eventProvider.level`

<!-- YAML
added: REPLACEME
-->

* {Object}

The minimum level descriptor. Set this property to a built-in level name or a
custom level descriptor to update the minimum level.

## `logger.levels`

<!-- YAML
added: REPLACEME
-->

* {Object}

Immutable descriptors for the built-in levels:

| Name    | Value |
| ------- | ----: |
| `trace` |    10 |
| `debug` |    20 |
| `info`  |    30 |
| `warn`  |    40 |
| `error` |    50 |
| `fatal` |    60 |

[`'defaultLoggerProviderChanged'`]: process.md#event-defaultloggerproviderchanged
[`--experimental-logger`]: cli.md#--experimental-logger
[`ConsoleProvider`]: #class-consoleprovider
[`JSON.stringify()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify
[`diagnostics_channel`]: diagnostics_channel.md
[`fs.Utf8Stream`]: fs.md#class-fsutf8stream
[`logger.create()`]: #loggercreateprovider-options
[`logger.getDefaultProvider()`]: #loggergetdefaultprovider
[`util.inspect()`]: util.md#utilinspectobject-options
