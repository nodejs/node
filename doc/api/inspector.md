# Inspector

<!--introduced_in=v8.0.0-->

> Stability: 2 - Stable

<!-- source_link=lib/inspector.js -->

The `node:inspector` module provides an API for interacting with the V8
inspector.

It can be accessed using:

```mjs
import * as inspector from 'node:inspector/promises';
```

```cjs
const inspector = require('node:inspector/promises');
```

or

```mjs
import * as inspector from 'node:inspector';
```

```cjs
const inspector = require('node:inspector');
```

## Promises API

<!-- YAML
added: v19.0.0
-->

> Stability: 1 - Experimental

### Class: `inspector.Session`

* Extends: {EventEmitter}

The `inspector.Session` is used for dispatching messages to the V8 inspector
back-end and receiving message responses and notifications.

#### `new inspector.Session()`

<!-- YAML
added: v8.0.0
-->

Create a new instance of the `inspector.Session` class. The inspector session
needs to be connected through [`session.connect()`][] before the messages
can be dispatched to the inspector backend.

When using `Session`, the object outputted by the console API will not be
released, unless we performed manually `Runtime.DiscardConsoleEntries`
command.

#### Event: `'inspectorNotification'`

<!-- YAML
added: v8.0.0
-->

* Type: {Object} The notification message object

Emitted when any notification from the V8 Inspector is received.

```js
session.on('inspectorNotification', (message) => console.log(message.method));
// Debugger.paused
// Debugger.resumed
```

> **Caveat** Breakpoints with same-thread session is not recommended, see
> [support of breakpoints][].

It is also possible to subscribe only to notifications with specific method:

#### Event: `<inspector-protocol-method>`

<!-- YAML
added: v8.0.0
-->

* Type: {Object} The notification message object

Emitted when an inspector notification is received that has its method field set
to the `<inspector-protocol-method>` value.

The following snippet installs a listener on the [`'Debugger.paused'`][]
event, and prints the reason for program suspension whenever program
execution is suspended (through breakpoints, for example):

```js
session.on('Debugger.paused', ({ params }) => {
  console.log(params.hitBreakpoints);
});
// [ '/the/file/that/has/the/breakpoint.js:11:0' ]
```

> **Caveat** Breakpoints with same-thread session is not recommended, see
> [support of breakpoints][].

#### `session.connect()`

<!-- YAML
added: v8.0.0
-->

Connects a session to the inspector back-end.

#### `session.connectToMainThread()`

<!-- YAML
added: v12.11.0
-->

Connects a session to the main thread inspector back-end. An exception will
be thrown if this API was not called on a Worker thread.

#### `session.disconnect()`

<!-- YAML
added: v8.0.0
-->

Immediately close the session. All pending message callbacks will be called
with an error. [`session.connect()`][] will need to be called to be able to send
messages again. Reconnected session will lose all inspector state, such as
enabled agents or configured breakpoints.

#### `session.post(method[, params])`

<!-- YAML
added: v19.0.0
-->

* `method` {string}
* `params` {Object}
* Returns: {Promise}

Posts a message to the inspector back-end.

```mjs
import { Session } from 'node:inspector/promises';
try {
  const session = new Session();
  session.connect();
  const result = await session.post('Runtime.evaluate', { expression: '2 + 2' });
  console.log(result);
} catch (error) {
  console.error(error);
}
// Output: { result: { type: 'number', value: 4, description: '4' } }
```

The latest version of the V8 inspector protocol is published on the
[Chrome DevTools Protocol Viewer][].

Node.js inspector supports all the Chrome DevTools Protocol domains declared
by V8. Chrome DevTools Protocol domain provides an interface for interacting
with one of the runtime agents used to inspect the application state and listen
to the run-time events.

#### Example usage

Apart from the debugger, various V8 Profilers are available through the DevTools
protocol.

##### CPU profiler

Here's an example showing how to use the [CPU Profiler][]:

```mjs
import { Session } from 'node:inspector/promises';
import fs from 'node:fs';
const session = new Session();
session.connect();

await session.post('Profiler.enable');
await session.post('Profiler.start');
// Invoke business logic under measurement here...

// some time later...
const { profile } = await session.post('Profiler.stop');

// Write profile to disk, upload, etc.
fs.writeFileSync('./profile.cpuprofile', JSON.stringify(profile));
```

##### Heap profiler

Here's an example showing how to use the [Heap Profiler][]:

```mjs
import { Session } from 'node:inspector/promises';
import fs from 'node:fs';
const session = new Session();

const fd = fs.openSync('profile.heapsnapshot', 'w');

session.connect();

session.on('HeapProfiler.addHeapSnapshotChunk', (m) => {
  fs.writeSync(fd, m.params.chunk);
});

const result = await session.post('HeapProfiler.takeHeapSnapshot', null);
console.log('HeapProfiler.takeHeapSnapshot done:', result);
session.disconnect();
fs.closeSync(fd);
```

## Callback API

### Class: `inspector.Session`

* Extends: {EventEmitter}

The `inspector.Session` is used for dispatching messages to the V8 inspector
back-end and receiving message responses and notifications.

#### `new inspector.Session()`

<!-- YAML
added: v8.0.0
-->

Create a new instance of the `inspector.Session` class. The inspector session
needs to be connected through [`session.connect()`][] before the messages
can be dispatched to the inspector backend.

When using `Session`, the object outputted by the console API will not be
released, unless we performed manually `Runtime.DiscardConsoleEntries`
command.

#### Event: `'inspectorNotification'`

<!-- YAML
added: v8.0.0
-->

* Type: {Object} The notification message object

Emitted when any notification from the V8 Inspector is received.

```js
session.on('inspectorNotification', (message) => console.log(message.method));
// Debugger.paused
// Debugger.resumed
```

> **Caveat** Breakpoints with same-thread session is not recommended, see
> [support of breakpoints][].

It is also possible to subscribe only to notifications with specific method:

#### Event: `<inspector-protocol-method>`;

<!-- YAML
added: v8.0.0
-->

* Type: {Object} The notification message object

Emitted when an inspector notification is received that has its method field set
to the `<inspector-protocol-method>` value.

The following snippet installs a listener on the [`'Debugger.paused'`][]
event, and prints the reason for program suspension whenever program
execution is suspended (through breakpoints, for example):

```js
session.on('Debugger.paused', ({ params }) => {
  console.log(params.hitBreakpoints);
});
// [ '/the/file/that/has/the/breakpoint.js:11:0' ]
```

> **Caveat** Breakpoints with same-thread session is not recommended, see
> [support of breakpoints][].

#### `session.connect()`

<!-- YAML
added: v8.0.0
-->

Connects a session to the inspector back-end.

#### `session.connectToMainThread()`

<!-- YAML
added: v12.11.0
-->

Connects a session to the main thread inspector back-end. An exception will
be thrown if this API was not called on a Worker thread.

#### `session.disconnect()`

<!-- YAML
added: v8.0.0
-->

Immediately close the session. All pending message callbacks will be called
with an error. [`session.connect()`][] will need to be called to be able to send
messages again. Reconnected session will lose all inspector state, such as
enabled agents or configured breakpoints.

#### `session.post(method[, params][, callback])`

<!-- YAML
added: v8.0.0
changes:
  - version: v18.0.0
    pr-url: https://github.com/nodejs/node/pull/41678
    description: Passing an invalid callback to the `callback` argument
                 now throws `ERR_INVALID_ARG_TYPE` instead of
                 `ERR_INVALID_CALLBACK`.
-->

* `method` {string}
* `params` {Object}
* `callback` {Function}

Posts a message to the inspector back-end. `callback` will be notified when
a response is received. `callback` is a function that accepts two optional
arguments: error and message-specific result.

```js
session.post('Runtime.evaluate', { expression: '2 + 2' },
             (error, { result }) => console.log(result));
// Output: { type: 'number', value: 4, description: '4' }
```

The latest version of the V8 inspector protocol is published on the
[Chrome DevTools Protocol Viewer][].

Node.js inspector supports all the Chrome DevTools Protocol domains declared
by V8. Chrome DevTools Protocol domain provides an interface for interacting
with one of the runtime agents used to inspect the application state and listen
to the run-time events.

You can not set `reportProgress` to `true` when sending a
`HeapProfiler.takeHeapSnapshot` or `HeapProfiler.stopTrackingHeapObjects`
command to V8.

#### Example usage

Apart from the debugger, various V8 Profilers are available through the DevTools
protocol.

##### CPU profiler

Here's an example showing how to use the [CPU Profiler][]:

```js
const inspector = require('node:inspector');
const fs = require('node:fs');
const session = new inspector.Session();
session.connect();

session.post('Profiler.enable', () => {
  session.post('Profiler.start', () => {
    // Invoke business logic under measurement here...

    // some time later...
    session.post('Profiler.stop', (err, { profile }) => {
      // Write profile to disk, upload, etc.
      if (!err) {
        fs.writeFileSync('./profile.cpuprofile', JSON.stringify(profile));
      }
    });
  });
});
```

##### Heap profiler

Here's an example showing how to use the [Heap Profiler][]:

```js
const inspector = require('node:inspector');
const fs = require('node:fs');
const session = new inspector.Session();

const fd = fs.openSync('profile.heapsnapshot', 'w');

session.connect();

session.on('HeapProfiler.addHeapSnapshotChunk', (m) => {
  fs.writeSync(fd, m.params.chunk);
});

session.post('HeapProfiler.takeHeapSnapshot', null, (err, r) => {
  console.log('HeapProfiler.takeHeapSnapshot done:', err, r);
  session.disconnect();
  fs.closeSync(fd);
});
```

## Common Objects

### `inspector.close()`

<!-- YAML
added: v9.0.0
changes:
  - version: v18.10.0
    pr-url: https://github.com/nodejs/node/pull/44489
    description: The API is exposed in the worker threads.
-->

Attempts to close all remaining connections, blocking the event loop until all
are closed. Once all connections are closed, deactivates the inspector.

### `inspector.console`

* Type: {Object} An object to send messages to the remote inspector console.

```js
require('node:inspector').console.log('a message');
```

The inspector console does not have API parity with Node.js
console.

### `inspector.open([port[, host[, wait]]])`

<!-- YAML
changes:
  - version: v20.6.0
    pr-url: https://github.com/nodejs/node/pull/48765
    description: inspector.open() now returns a `Disposable` object.
-->

* `port` {number} Port to listen on for inspector connections. Optional.
  **Default:** what was specified on the CLI.
* `host` {string} Host to listen on for inspector connections. Optional.
  **Default:** what was specified on the CLI.
* `wait` {boolean} Block until a client has connected. Optional.
  **Default:** `false`.
* Returns: {Disposable} A Disposable that calls [`inspector.close()`][].

Activate inspector on host and port. Equivalent to
`node --inspect=[[host:]port]`, but can be done programmatically after node has
started.

If wait is `true`, will block until a client has connected to the inspect port
and flow control has been passed to the debugger client.

See the [security warning][] regarding the `host`
parameter usage.

### `inspector.url()`

* Returns: {string|undefined}

Return the URL of the active inspector, or `undefined` if there is none.

```console
$ node --inspect -p 'inspector.url()'
Debugger listening on ws://127.0.0.1:9229/166e272e-7a30-4d09-97ce-f1c012b43c34
For help, see: https://nodejs.org/en/docs/inspector
ws://127.0.0.1:9229/166e272e-7a30-4d09-97ce-f1c012b43c34

$ node --inspect=localhost:3000 -p 'inspector.url()'
Debugger listening on ws://localhost:3000/51cf8d0e-3c36-4c59-8efd-54519839e56a
For help, see: https://nodejs.org/en/docs/inspector
ws://localhost:3000/51cf8d0e-3c36-4c59-8efd-54519839e56a

$ node -p 'inspector.url()'
undefined
```

### `inspector.waitForDebugger()`

<!-- YAML
added: v12.7.0
-->

Blocks until a client (existing or connected later) has sent
`Runtime.runIfWaitingForDebugger` command.

An exception will be thrown if there is no active inspector.

## Integration with DevTools

> Stability: 1.1 - Active development

The `node:inspector` module provides an API for integrating with devtools that support Chrome DevTools Protocol.
DevTools frontends connected to a running Node.js instance can capture protocol events emitted from the instance
and display them accordingly to facilitate debugging.
The following methods broadcast a protocol event to all connected frontends.
The `params` passed to the methods can be optional, depending on the protocol.

```js
// The `Network.requestWillBeSent` event will be fired.
inspector.Network.requestWillBeSent({
  requestId: 'request-id-1',
  timestamp: Date.now() / 1000,
  wallTime: Date.now(),
  request: {
    url: 'https://nodejs.org/en',
    method: 'GET',
  },
});
```

### `inspector.Network.dataReceived([params])`

<!-- YAML
added:
 - v24.2.0
 - v22.17.0
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.dataReceived` event to connected frontends, or buffers the data if
`Network.streamResourceContent` command was not invoked for the given request yet.

Also enables `Network.getResponseBody` command to retrieve the response data.

### `inspector.Network.dataSent([params])`

<!-- YAML
added:
  - v24.3.0
  - v22.18.0
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Enables `Network.getRequestPostData` command to retrieve the request data.

### `inspector.Network.requestWillBeSent([params])`

<!-- YAML
added:
 - v22.6.0
 - v20.18.0
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.requestWillBeSent` event to connected frontends. This event indicates that
the application is about to send an HTTP request.

### `inspector.Network.responseReceived([params])`

<!-- YAML
added:
 - v22.6.0
 - v20.18.0
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.responseReceived` event to connected frontends. This event indicates that
HTTP response is available.

### `inspector.Network.loadingFinished([params])`

<!-- YAML
added:
 - v22.6.0
 - v20.18.0
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.loadingFinished` event to connected frontends. This event indicates that
HTTP request has finished loading.

### `inspector.Network.loadingFailed([params])`

<!-- YAML
added:
 - v22.7.0
 - v20.18.0
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.loadingFailed` event to connected frontends. This event indicates that
HTTP request has failed to load.

### `inspector.Network.webSocketCreated([params])`

<!-- YAML
added:
  - REPLACEME
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.webSocketCreated` event to connected frontends. This event indicates that
a WebSocket connection has been initiated.

### `inspector.Network.webSocketHandshakeResponseReceived([params])`

<!-- YAML
added:
  - REPLACEME
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.webSocketHandshakeResponseReceived` event to connected frontends.
This event indicates that the WebSocket handshake response has been received.

### `inspector.Network.webSocketClosed([params])`

<!-- YAML
added:
  - REPLACEME
-->

* `params` {Object}

This feature is only available with the `--experimental-network-inspection` flag enabled.

Broadcasts the `Network.webSocketClosed` event to connected frontends.
This event indicates that a WebSocket connection has been closed.

### `inspector.NetworkResources.put`

<!-- YAML
added:
  - v24.5.0
-->

> Stability: 1.1 - Active Development

This feature is only available with the `--experimental-inspector-network-resource` flag enabled.

The inspector.NetworkResources.put method is used to provide a response for a loadNetworkResource
request issued via the Chrome DevTools Protocol (CDP).
This is typically triggered when a source map is specified by URL, and a DevTools frontend—such as
Chrome—requests the resource to retrieve the source map.

This method allows developers to predefine the resource content to be served in response to such CDP requests.

```js
const inspector = require('node:inspector');
// By preemptively calling put to register the resource, a source map can be resolved when
// a loadNetworkResource request is made from the frontend.
async function setNetworkResources() {
  const mapUrl = 'http://localhost:3000/dist/app.js.map';
  const tsUrl = 'http://localhost:3000/src/app.ts';
  const distAppJsMap = await fetch(mapUrl).then((res) => res.text());
  const srcAppTs = await fetch(tsUrl).then((res) => res.text());
  inspector.NetworkResources.put(mapUrl, distAppJsMap);
  inspector.NetworkResources.put(tsUrl, srcAppTs);
};
setNetworkResources().then(() => {
  require('./dist/app');
});
```

For more details, see the official CDP documentation: [Network.loadNetworkResource](https://chromedevtools.github.io/devtools-protocol/tot/Network/#method-loadNetworkResource)

## Support of breakpoints

The Chrome DevTools Protocol [`Debugger` domain][] allows an
`inspector.Session` to attach to a program and set breakpoints to step through
the codes.

However, setting breakpoints with a same-thread `inspector.Session`, which is
connected by [`session.connect()`][], should be avoided as the program being
attached and paused is exactly the debugger itself. Instead, try connect to the
main thread by [`session.connectToMainThread()`][] and set breakpoints in a
worker thread, or connect with a [Debugger][] program over WebSocket
connection.

[CPU Profiler]: https://chromedevtools.github.io/devtools-protocol/v8/Profiler
[Chrome DevTools Protocol Viewer]: https://chromedevtools.github.io/devtools-protocol/v8/
[Debugger]: debugger.md
[Heap Profiler]: https://chromedevtools.github.io/devtools-protocol/v8/HeapProfiler
[`'Debugger.paused'`]: https://chromedevtools.github.io/devtools-protocol/v8/Debugger#event-paused
[`Debugger` domain]: https://chromedevtools.github.io/devtools-protocol/v8/Debugger
[`inspector.close()`]: #inspectorclose
[`session.connect()`]: #sessionconnect
[`session.connectToMainThread()`]: #sessionconnecttomainthread
[security warning]: cli.md#warning-binding-inspector-to-a-public-ipport-combination-is-insecure
[support of breakpoints]: #support-of-breakpoints
