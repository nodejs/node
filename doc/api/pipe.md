# Pipe

<!--introduced_in=REPLACEME-->

> Stability: 1.1 - Active development

<!-- source_link=lib/pipe.js -->

The `node:pipe` module provides APIs for creating operating system pipes.

It can be accessed using:

```mjs
import pipe from 'node:pipe';
```

```cjs
const pipe = require('node:pipe');
```

## `pipe.createPipe()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object}
  * `readable` {net.Socket} The readable end of the pipe.
  * `writable` {net.Socket} The writable end of the pipe.

The `pipe.createPipe()` method creates an operating system pipe pair. The
returned `readable` and `writable` streams are owned by the current process and
may be passed to [`child_process.spawn()`][] using the [`stdio`][] option.

When a `readable` endpoint is passed as child stdin or as another child fd, the
child leases a readable handle. When a `writable` endpoint is passed as child
stdout, stderr, or another child fd, the child leases a writable handle. A
`readable` endpoint may not be passed as child stdout or stderr, and a
`writable` endpoint may not be passed as child stdin. An endpoint may be leased
to only one child process at a time. After the child process exits, endpoints
created by [`pipe.createPipe()`][] are released from their lease and may be
passed to another [`child_process.spawn()`][] call.
Endpoints created by [`pipe.createPipe()`][] are not supported by synchronous
child process APIs such as [`child_process.spawnSync()`][].

A `readable` endpoint created by [`pipe.createPipe()`][] must not be flowing
when it is passed to [`child_process.spawn()`][]. The child process
[`'close'`][] event does not wait for such an endpoint to close and does not
resume it after the child process exits.

The current process is responsible for the endpoint streams. Use normal stream
idioms such as `end()` to finish writing and stream consumption to drain a
readable endpoint. Use `resume()` when an unread readable endpoint should be
drained without observing its data, and use `destroy()` when an endpoint is no
longer needed without being naturally ended or drained.

```cjs
const { spawn } = require('node:child_process');
const { createPipe } = require('node:pipe');
const { text } = require('node:stream/consumers');

const { readable, writable } = createPipe();
const child = spawn(process.execPath, ['-e', `
  const fs = require('node:fs');
  const buffer = Buffer.alloc(1);
  const count = fs.readSync(0, buffer, 0, 1, null);
  fs.writeSync(1, buffer.subarray(0, count));
`], {
  stdio: [readable, 'pipe', 'inherit'],
});

const output = text(child.stdout);
writable.end('abc');

child.on('close', async () => {
  console.log(await output); // Prints: a
  console.log(await text(readable)); // Prints: bc
});
```

```mjs
import { spawn } from 'node:child_process';
import { createPipe } from 'node:pipe';
import { text } from 'node:stream/consumers';

const { readable, writable } = createPipe();
const child = spawn(process.execPath, ['-e', `
  const fs = require('node:fs');
  const buffer = Buffer.alloc(1);
  const count = fs.readSync(0, buffer, 0, 1, null);
  fs.writeSync(1, buffer.subarray(0, count));
`], {
  stdio: [readable, 'pipe', 'inherit'],
});

const output = text(child.stdout);
writable.end('abc');

child.on('close', async () => {
  console.log(await output); // Prints: a
  console.log(await text(readable)); // Prints: bc
});
```

[`'close'`]: child_process.md#event-close
[`child_process.spawn()`]: child_process.md#child_processspawncommand-args-options
[`child_process.spawnSync()`]: child_process.md#child_processspawnsynccommand-args-options
[`net.Socket`]: net.md#class-netsocket
[`stdio`]: child_process.md#optionsstdio
[`writable.destroy()`]: stream.md#writabledestroyerror
[`writable.end()`]: stream.md#writableendchunk-encoding-callback
