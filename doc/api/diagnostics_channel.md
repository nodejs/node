# Diagnostics Channel

<!--introduced_in=v15.1.0-->

> Stability: 1 - Experimental

<!-- source_link=lib/diagnostics_channel.js -->

The `node:diagnostics_channel` module provides an API to create named channels
to report arbitrary message data for diagnostics purposes.

It can be accessed using:

```mjs
import diagnostics_channel from 'node:diagnostics_channel';
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');
```

It is intended that a module writer wanting to report diagnostics messages
will create one or many top-level channels to report messages through.
Channels may also be acquired at runtime but it is not encouraged
due to the additional overhead of doing so. Channels may be exported for
convenience, but as long as the name is known it can be acquired anywhere.

If you intend for your module to produce diagnostics data for others to
consume it is recommended that you include documentation of what named
channels are used along with the shape of the message data. Channel names
should generally include the module name to avoid collisions with data from
other modules.

## Public API

### Overview

Following is a simple overview of the public API.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';

// Get a reusable channel object
const channel = diagnostics_channel.channel('my-channel');

function onMessage(message, name) {
  // Received data
}

// Subscribe to the channel
diagnostics_channel.subscribe('my-channel', onMessage);

// Check if the channel has an active subscriber
if (channel.hasSubscribers) {
  // Publish data to the channel
  channel.publish({
    some: 'data'
  });
}

// Unsubscribe from the channel
diagnostics_channel.unsubscribe('my-channel', onMessage);

import { AsyncLocalStorage } from 'node:async_hooks';

// Create a storage channel
const storageChannel = diagnostics_channel.storageChannel('my-channel');
const storage = new AsyncLocalStorage();

// Bind an AsyncLocalStorage instance to the channel. This runs the storage
// with input from the channel as the context data.
storageChannel.bindStore(storage, (name) => {
  return `Hello, ${name}`;
});

// Runs the bound storage with the given channel input.
const message = storageChannel.run('world', () => {
  return storage.getStore();
});
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');

// Get a reusable channel object
const channel = diagnostics_channel.channel('my-channel');

function onMessage(message, name) {
  // Received data
}

// Subscribe to the channel
diagnostics_channel.subscribe('my-channel', onMessage);

// Check if the channel has an active subscriber
if (channel.hasSubscribers) {
  // Publish data to the channel
  channel.publish({
    some: 'data'
  });
}

// Unsubscribe from the channel
diagnostics_channel.unsubscribe('my-channel', onMessage);

const { AsyncLocalStorage } = require('node:async_hooks');

// Create a storage channel
const storageChannel = diagnostics_channel.storageChannel('my-channel');
const storage = new AsyncLocalStorage();

// Bind an AsyncLocalStorage instance to the channel. This runs the storage
// with input from the channel as the context data.
storageChannel.bindStore(storage, (name) => {
  return `Hello, ${name}`;
});

// Runs the bound storage with the given channel input.
const message = storageChannel.run('world', () => {
  return storage.getStore();
});
```

#### `diagnostics_channel.hasSubscribers(name)`

<!-- YAML
added:
 - v15.1.0
 - v14.17.0
-->

* `name` {string|symbol} The channel name
* Returns: {boolean} If there are active subscribers

Check if there are active subscribers to the named channel. This is helpful if
the message you want to send might be expensive to prepare.

This API is optional but helpful when trying to publish messages from very
performance-sensitive code.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';

if (diagnostics_channel.hasSubscribers('my-channel')) {
  // There are subscribers, prepare and publish message
}
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');

if (diagnostics_channel.hasSubscribers('my-channel')) {
  // There are subscribers, prepare and publish message
}
```

#### `diagnostics_channel.channel(name)`

<!-- YAML
added:
 - v15.1.0
 - v14.17.0
-->

* `name` {string|symbol} The channel name
* Returns: {Channel} The named channel object

This is the primary entry-point for anyone wanting to publish to a named
channel. It produces a channel object which is optimized to reduce overhead at
publish time as much as possible.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';

const channel = diagnostics_channel.channel('my-channel');
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');

const channel = diagnostics_channel.channel('my-channel');
```

#### `diagnostics_channel.subscribe(name, onMessage)`

<!-- YAML
added:
 - v18.7.0
 - v16.17.0
-->

* `name` {string|symbol} The channel name
* `onMessage` {Function} The handler to receive channel messages
  * `message` {any} The message data
  * `name` {string|symbol} The name of the channel

Register a message handler to subscribe to this channel. This message handler
will be run synchronously whenever a message is published to the channel. Any
errors thrown in the message handler will trigger an [`'uncaughtException'`][].

```mjs
import diagnostics_channel from 'diagnostics_channel';

diagnostics_channel.subscribe('my-channel', (message, name) => {
  // Received data
});
```

```cjs
const diagnostics_channel = require('diagnostics_channel');

diagnostics_channel.subscribe('my-channel', (message, name) => {
  // Received data
});
```

#### `diagnostics_channel.unsubscribe(name, onMessage)`

<!-- YAML
added:
 - v18.7.0
 - v16.17.0
-->

* `name` {string|symbol} The channel name
* `onMessage` {Function} The previous subscribed handler to remove
* Returns: {boolean} `true` if the handler was found, `false` otherwise.

Remove a message handler previously registered to this channel with
[`diagnostics_channel.subscribe(name, onMessage)`][].

```mjs
import diagnostics_channel from 'diagnostics_channel';

function onMessage(message, name) {
  // Received data
}

diagnostics_channel.subscribe('my-channel', onMessage);

diagnostics_channel.unsubscribe('my-channel', onMessage);
```

```cjs
const diagnostics_channel = require('diagnostics_channel');

function onMessage(message, name) {
  // Received data
}

diagnostics_channel.subscribe('my-channel', onMessage);

diagnostics_channel.unsubscribe('my-channel', onMessage);
```

#### `diagnostics_channel.storageChannel(name)`

<!-- YAML
added:
 - REPLACEME
-->

* `name` {string|symbol} The channel name
* Returns: {StorageChannel} named StorageChannel

A [`StorageChannel`][] is used to bind diagnostics\_channel inputs as the
context for an [`AsyncLocalStorage`][] instance.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';
import { AsyncLocalStorage } from 'node:async_hooks';

const channel = diagnostics_channel.storageChannel('my-channel');
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

const channel = diagnostics_channel.storageChannel('my-channel');
```

#### `diagnostics_channel.bindStore(name, store[, builder])`

<!-- YAML
added:
 - REPLACEME
-->

* `name` {string|symbol} The channel name
* `store` {AsyncLocalStorage} The storage to bind the the storage channel
* `builder` {Function} Optional function to transform data to storage context
* Returns: {boolean} `true` if storage was not already bound, `false` otherwise.

Binds an [`AsyncLocalStorage`][] instance to the [`StorageChannel`][] such that
inputs to [`storageChannel.run(data, handler)`][] will run the store with the
given input data as the context.

An optional builder function can be provided to perform transformations on
the data given by the channel before being set as the storage context data.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';
import { AsyncLocalStorage } from 'node:async_hooks';

const storage = new AsyncLocalStorage();

// Stores the channel input in a `channelInput` property of the context.
diagnostics_channel.bindStore('my-channel', storage, (channelInput) => {
  return { channelInput };
});
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

const storage = new AsyncLocalStorage();

// Stores the channel input in a `channelInput` property of the context.
diagnostics_channel.bindStore('my-channel', storage, (channelInput) => {
  return { channelInput };
});
```

#### `diagnostics_channel.unbindStore(name, store)`

* `name` {string|symbol} The channel name
* `store` {AsyncLocalStorage} a store to unbind from the channel
* Returns: {boolean} `true` if the store was previously bound, otherwise `false`

Unbinds an [`AsyncLocalStorage`][] instance from the [`StorageChannel`][]. After
doing this channel inputs will no longer be used to create contexts for the
store to run with.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';
import { AsyncLocalStorage } from 'node:async_hooks';

const storage = new AsyncLocalStorage();

// Stop using channel inputs to initiate new context runs on the storage.
diagnostics_channel.unbindStore('my-channel', storage);
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

const storage = new AsyncLocalStorage();

// Stop using channel inputs to initiate new context runs on the storage.
diagnostics_channel.unbindStore('my-channel', storage);
```

### Class: `Channel`

<!-- YAML
added:
 - v15.1.0
 - v14.17.0
-->

The class `Channel` represents an individual named channel within the data
pipeline. It is used to track subscribers and to publish messages when there
are subscribers present. It exists as a separate object to avoid channel
lookups at publish time, enabling very fast publish speeds and allowing
for heavy use while incurring very minimal cost. Channels are created with
[`diagnostics_channel.channel(name)`][], constructing a channel directly
with `new Channel(name)` is not supported.

#### `channel.hasSubscribers`

<!-- YAML
added:
 - v15.1.0
 - v14.17.0
-->

* Returns: {boolean} If there are active subscribers

Check if there are active subscribers to this channel. This is helpful if
the message you want to send might be expensive to prepare.

This API is optional but helpful when trying to publish messages from very
performance-sensitive code.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';

const channel = diagnostics_channel.channel('my-channel');

if (channel.hasSubscribers) {
  // There are subscribers, prepare and publish message
}
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');

const channel = diagnostics_channel.channel('my-channel');

if (channel.hasSubscribers) {
  // There are subscribers, prepare and publish message
}
```

#### `channel.publish(message)`

<!-- YAML
added:
 - v15.1.0
 - v14.17.0
-->

* `message` {any} The message to send to the channel subscribers

Publish a message to any subscribers to the channel. This will trigger
message handlers synchronously so they will execute within the same context.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';

const channel = diagnostics_channel.channel('my-channel');

channel.publish({
  some: 'message'
});
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');

const channel = diagnostics_channel.channel('my-channel');

channel.publish({
  some: 'message'
});
```

#### `channel.subscribe(onMessage)`

<!-- YAML
added:
 - v15.1.0
 - v14.17.0
deprecated:
 - v18.7.0
 - v16.17.0
-->

> Stability: 0 - Deprecated: Use [`diagnostics_channel.subscribe(name, onMessage)`][]

* `onMessage` {Function} The handler to receive channel messages
  * `message` {any} The message data
  * `name` {string|symbol} The name of the channel

Register a message handler to subscribe to this channel. This message handler
will be run synchronously whenever a message is published to the channel. Any
errors thrown in the message handler will trigger an [`'uncaughtException'`][].

```mjs
import diagnostics_channel from 'node:diagnostics_channel';

const channel = diagnostics_channel.channel('my-channel');

channel.subscribe((message, name) => {
  // Received data
});
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');

const channel = diagnostics_channel.channel('my-channel');

channel.subscribe((message, name) => {
  // Received data
});
```

#### `channel.unsubscribe(onMessage)`

<!-- YAML
added:
 - v15.1.0
 - v14.17.0
deprecated:
 - v18.7.0
 - v16.17.0
changes:
  - version:
    - v17.1.0
    - v16.14.0
    - v14.19.0
    pr-url: https://github.com/nodejs/node/pull/40433
    description: Added return value. Added to channels without subscribers.
-->

> Stability: 0 - Deprecated: Use [`diagnostics_channel.unsubscribe(name, onMessage)`][]

* `onMessage` {Function} The previous subscribed handler to remove
* Returns: {boolean} `true` if the handler was found, `false` otherwise.

Remove a message handler previously registered to this channel with
[`channel.subscribe(onMessage)`][].

```mjs
import diagnostics_channel from 'node:diagnostics_channel';

const channel = diagnostics_channel.channel('my-channel');

function onMessage(message, name) {
  // Received data
}

channel.subscribe(onMessage);

channel.unsubscribe(onMessage);
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');

const channel = diagnostics_channel.channel('my-channel');

function onMessage(message, name) {
  // Received data
}

channel.subscribe(onMessage);

channel.unsubscribe(onMessage);
```

### Class: `StorageChannel`

<!-- YAML
added:
 - REPLACEME
-->

The class `StorageChannel` represents a pair of named channels within the data
pipeline. It is used to encapsulate a context in which an `AsyncLocalStorage`
should have a bound value.

#### `storageChannel.isBoundToStore(store)`

* `store` {AsyncLocalStorage} a store that may or may not be bound
* Returns: {boolean}  `true` if store is bound to channel, `false` otherwise.

Checks if the given store is already bound to the [`StorageChannel`][].

```mjs
import diagnostics_channel from 'node:diagnostics_channel';
import { AsyncLocalStorage } from 'node:async_hooks';

const storageChannel = diagnostics_channel.storageChannel('my-channel');
const storage = new AsyncLocalStorage();

if (storageChannel.isBoundToStore(storage)) {
  // The storage is already bound to storageChannel
}
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

const storageChannel = diagnostics_channel.storageChannel('my-channel');
const storage = new AsyncLocalStorage();

if (storageChannel.isBoundToStore(storage)) {
  // The storage is already bound to storageChannel
}
```

#### `storageChannel.run(data, handler)`

* `data` {any} data to pass to any bound stores as context input
* `handler` {Function} a scope function in which the store should run
* Returns: {any} returns the return value of the given handler function

While the handler function is running, any bound storages will be given the
data as input to run the storage context.

```mjs
import diagnostics_channel from 'node:diagnostics_channel';
import { AsyncLocalStorage } from 'node:async_hooks';

const storageChannel = diagnostics_channel.storageChannel('my-channel');

// Create and bind a storage to the storage channel
const storage = new AsyncLocalStorage();
storageChannel.bind(storage);

// The storage will be run with the given data
storageChannel.run({ my: 'context' }, () => {
  if (storage.getStore().my === 'context') {
    // The given context will be used within this function
  }
});
```

```cjs
const diagnostics_channel = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

const storageChannel = diagnostics_channel.storageChannel('my-channel');

// Create and bind a storage to the storage channel
const storage = new AsyncLocalStorage();
storageChannel.bind(storage);

// The storage will be run with the given data
storageChannel.run({ my: 'context' }, () => {
  if (storage.getStore().my === 'context') {
    // The given context will be used within this function
  }
});
```

### Built-in Channels

#### HTTP

`http.client.request.start`

* `request` {http.ClientRequest}

Emitted when client starts a request.

`http.client.response.finish`

* `request` {http.ClientRequest}
* `response` {http.IncomingMessage}

Emitted when client receives a response.

`http.server.request.start`

* `request` {http.IncomingMessage}
* `response` {http.ServerResponse}
* `socket` {net.Socket}
* `server` {http.Server}

Emitted when server receives a request.

`http.server.response.finish`

* `request` {http.IncomingMessage}
* `response` {http.ServerResponse}
* `socket` {net.Socket}
* `server` {http.Server}

Emitted when server sends a response.

#### NET

`net.client.socket`

* `socket` {net.Socket}

Emitted when a new TCP or pipe client socket is created.

`net.server.socket`

* `socket` {net.Socket}

Emitted when a new TCP or pipe connection is received.

#### UDP

`udp.socket`

* `socket` {dgram.Socket}

Emitted when a new UDP socket is created.

#### Process

<!-- YAML
added: REPLACEME
-->

`child_process`

* `process` {ChildProcess}

Emitted when a new process is created.

#### Worker Thread

<!-- YAML
added: REPLACEME
-->

`worker_threads`

* `worker` [`Worker`][]

Emitted when a new thread is created.

[`'uncaughtException'`]: process.md#event-uncaughtexception
[`AsyncLocalStorage`]: async_context.md#class-asynclocalstorage
[`StorageChannel`]: #class-storagechannel
[`Worker`]: worker_threads.md#class-worker
[`channel.subscribe(onMessage)`]: #channelsubscribeonmessage
[`diagnostics_channel.channel(name)`]: #diagnostics_channelchannelname
[`diagnostics_channel.subscribe(name, onMessage)`]: #diagnostics_channelsubscribename-onmessage
[`diagnostics_channel.unsubscribe(name, onMessage)`]: #diagnostics_channelunsubscribename-onmessage
[`storageChannel.run(data, handler)`]: #storagechannelrundata-handler
