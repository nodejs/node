# Cluster

    Stability: 2 - Stable

Node.js executes JavaScript code within the context of an event loop. Under the 
hood, non-blocking socket I/O runs on the main event loop thread while blocking 
DNS and filesystem I/O runs on a secondary thread that notifies the main thread 
when it's finished. This concurrency model allows a blocking 
[I/O bound](https://en.wikipedia.org/wiki/I/O_bound) application to easily 
distribute work across multiple CPU cores. For more details, see the 
[libuv](http://docs.libuv.org/en/v1.x/design.html) project.

The `cluster` API allows a non-blocking I/O bound application to distribute 
work across multiple CPU cores. A Node.js cluster is created when a **master 
process** forks one or more **worker processes**. By design, workers use the 
same application code as the master. Conveniently, the `cluster.isMaster` 
property determines if a running process is the master or a worker.

```js
const cluster = require('cluster');
if (cluster.isMaster) {
  for (var i = 0; i < numCPUs; i++) {
    cluster.fork();
  }
} else {
  // Non-blocking I/O work
}
```

The implementation itself is built directly on top of the
[`child_process.fork()`][] API, The master communicates with, and manages,
each of the workers through an IPC channel.

## Distributing server workload

One of the primary use cases for the cluster API is allowing a developer to
easily distribute request processing over multiple worker processes that all
share a single common server port:

```js
const cluster = require('cluster');
const http = require('http');
const numCPUs = require('os').cpus().length;

if (cluster.isMaster) {
  // Fork workers.
  for (var i = 0; i < numCPUs; i++) {
    cluster.fork();
  }

  cluster.on('exit', (worker, code, signal) => {
    console.log(`worker ${worker.process.pid} died`);
  });
} else {
  // Workers can share any TCP connection
  // In this case it is an HTTP server
  http.createServer((req, res) => {
    res.writeHead(200);
    res.end('hello world\n');
  }).listen(8000);
}
```

Running this example will now share port 8000 between the workers:

```
$ NODE_DEBUG=cluster node server.js
23521,Master Worker 23524 online
23521,Master Worker 23526 online
23521,Master Worker 23523 online
23521,Master Worker 23528 online
```

Please note that, on Windows, it is not yet possible to set up a named pipe
server in a worker.

### How It Works

The implementation of the `cluster` API in Node.js includes the built-in ability
to share TCP server connections automatically across multiple worker forks.
Whenever a worker starts and begins listening on a port, the master
is actually directed to open and listen to that port, distributing incoming
connections among the various children.

The cluster module supports two methods of distributing incoming connections:

1. The first is a round-robin model where the master process listens on a
   port, accepts new connections, and distributes them across all workers
   in a round-robin fashion, with some built-in intelligence to avoid
   overloading any single worker process. This is the default on all platforms
   except Windows),

2. The second approach is where the master process creates the listen
   socket and sends it to interested workers. The workers then accept
   incoming connections directly.

The second approach should, in theory, give the best performance.
In practice however, distribution tends to be very unbalanced due
to operating system scheduler vagaries. Loads have been observed
where over 70% of all connections ended up in just two processes,
out of a total of eight.

When running within a worker process, `server.listen()` hands off most of
its work to the master process. There are three cases where the behavior
between a normal Node.js process and a cluster worker differs:

1. `server.listen({fd: 7})` Because the message is passed to the master,
   file descriptor 7 **in the parent** will be listened on, and the
   handle passed to the worker, rather than listening to the worker's
   idea of what the number 7 file descriptor references.
2. `server.listen(handle)` Listening on handles explicitly will cause
   the worker to use the supplied handle, rather than talk to the master
   process.  If the worker already has the handle, then it's presumed
   that you know what you are doing.
3. `server.listen(0)` Normally, this will cause servers to listen on a
   random port.  However, in a cluster, each worker will receive the
   same "random" port each time they do `listen(0)`.  In essence, the
   port is random the first time, but predictable thereafter.  If you
   want to listen on a unique port, generate a port number based on the
   cluster worker ID.

It is important to keep in mind that worker processes do not share state or
memory with each other, or with the master, beyond the shared socket handles
used for distributing connection workload. Applications using cluster must be
written in such a way that they do not rely too heavily on in-memory data
objects for things like sessions and login.

Because workers are all separate processes, it is safe to terminate and
re-spawn individual workers depending on the program's needs, without
affecting any of the other workers. As long as there are some workers still
alive, the server will continue to accept connections.  If no workers are
alive, existing connections will be dropped and new connections will be
refused.  Node.js does not automatically manage the number of workers for you,
however.  It is your responsibility to manage the worker pool for your
application's needs.

## The `cluster` module

The `cluster` module is loaded using `require('custer')`:

```js
const cluster = require('cluster');
```

The `cluster` module itself is an instance of `'EventEmitter'` and exposes a
number of properties and methods whose values and functions will vary depending
on if the current process is the master or a worker.

### Event: 'disconnect'

* `worker` {cluster.Worker}

Emitted within the master process after a worker IPC channel has disconnected.
This can occur when a worker exits gracefully, is killed, or is disconnected
manually (such as when using `worker.disconnect()`).

There can be a delay between the `'disconnect'` and `'exit'` events.  These
events can be used, for instance, to detect when a working process is stuck in
a cleanup process or if there are long-living connections.

When triggered, the callback function will be passed the `Worker` instance
representing the worker that has disconnected:

```js
cluster.on('disconnect', (worker) => {
  console.log(`The worker #${worker.id} has disconnected`);
});
```

### Event: 'exit'

* `worker` {cluster.Worker}
* `code` {Number} the exit code if the child exited on its own.
* `signal` {String} the signal by which the child process was terminated.

When any worker processes exits or is terminated for any reason, the cluster
module will emit the `'exit'` event within the master process.

This can be used, for instance, to restart a failing worker by calling
`cluster.fork()` again.

```js
cluster.on('exit', (worker, code, signal) => {
  console.log('worker %d died (%s). restarting...',
    worker.process.pid, signal || code);
  cluster.fork();
});
```

See [child_process event: 'exit'][].

### Event: 'fork'

* `worker` {cluster.Worker}

When a new worker is forked the cluster module will emit a `'fork'` event
within the master process.

This can be used, for instance, to log worker activity or to create timeouts
for managing worker lifetime:

```js
const timeouts = [];
function errorMsg() {
  console.error('Something must be wrong with the connection ...');
}

cluster.on('fork', (worker) => {
  timeouts[worker.id] = setTimeout(errorMsg, 2000);
});
cluster.on('listening', (worker, address) => {
  clearTimeout(timeouts[worker.id]);
});
cluster.on('exit', (worker, code, signal) => {
  clearTimeout(timeouts[worker.id]);
  errorMsg();
});
```

### Event: 'listening'

* `worker` {cluster.Worker}
* `address` {Object}

After calling the `listen()` event on a TCP server inside a worker, whenever
the `'listening'` event is emitted by the server object, a corresponding
`'listening'` event will also be emitted by the `cluster` module in the
master process.

The event handler is passed two arguments when invoked: `worker` and `address`.
The `worker` argument is a reference to the `Worker` object, and the `address`
argument is an object containing the properties `address`, `port`, and
`addressType`. These are particularly useful if the worker is listening
on more than one address.

```js
cluster.on('listening', (worker, address) => {
  console.log(
    `Worker ${worker.id} is now connected to ` +
    `${address.address}:${address.port}`);
});
```

The `addressType` is one of:

* `4` (TCPv4)
* `6` (TCPv6)
* `-1` (local domain)
* `"udp4"` or `"udp6"` (UDP v4 or v6)

### Event: 'message'

* `worker` {cluster.Worker}
* `message` {Object}
* `handle` {undefined|Object}

Emitted when any worker receives a message.

See [child_process event: 'message'][].

Before Node.js v6.0, this event emitted only the message and the handle,
but not the worker object, contrary to what the documentation stated.

If you need to support older versions and don't need the worker object,
you can work around the discrepancy by checking the number of arguments:

```js
cluster.on('message', function(worker, message, handle) {
  if (arguments.length === 2) {
    handle = message;
    message = worker;
    worker = undefined;
  }
  // ...
});
```

### Event: 'online'

* `worker` {cluster.Worker}

The `'online'` event will be emitted in the master process whenever a
worker has fully started and is ready to begin accepting messages from the
master.

The difference between `'fork'` and `'online'` is that `'fork'` is emitted when
the master spawns a worker, while `'online'` is emitted only when the worker is
running.

```js
cluster.on('online', (worker) => {
  console.log('Yay, the worker responded after it was spawned');
});
```

### Event: 'setup'

* `settings` {Object}

The `'setup'` event is emitted  in the master process whenever
`cluster.setupMaster()` is called.

The event handler is passed a `settings` argument that is set to the value of
the `cluster.settings` property at the time `cluster.setupMaster()` was called
and is advisory only since multiple calls to `cluster.setupMaster()` can
occur within a single tick. If accuracy is important, use `cluster.settings`.

### cluster.disconnect([callback])

* `callback` {Function} called when all workers are disconnected and handles are
  closed

Calling the `cluster.disconnect()` method will sequentially call the
`worker.disconnect()` method on each `Worker` instance in `cluster.workers`.

Once the workers are disconnected, all internal handles will be closed,
allowing the master process to exit gracefully if there is no other pending
work.

The method takes an optional `callback` argument which will be called when
all workers have been disconnected.

The `cluster.disconnect()` method can only be called from the master process.

### cluster.fork([env])

* `env` {Object} Key/value pairs to add to worker process environment.
* return {Worker object}

Spawn a new worker process.

The `cluster.fork()` method can only be called from the master process.

### cluster.isMaster

* {Boolean}

Returns `true` only if the process is a master.

This state is determined by checking the value of `process.env.NODE_UNIQUE_ID`.
If `process.env.NODE_UNIQUE_ID` is `undefined`, then `cluster.isMaster` is
`true`.

### cluster.isWorker

* {Boolean}

Returns `true` only if the process is not a master (e.g. `cluster.isMaster`
is `false`);

### cluster.schedulingPolicy

Identifies the scheduling policy. This will either be `cluster.SCHED_RR` for
round-robin or `cluster.SCHED_NONE` for the operating system default. This is a
global setting that cannot be modified once either the first worker is spawned
or `cluster.setupMaster()` is called (whichever comes first).

`SCHED_RR` is the default on all operating systems except Windows.
Windows will change to `SCHED_RR` once libuv is able to effectively
distribute IOCP handles without incurring a large performance hit.

It is possible to set the value of `cluster.schedulingPolicy` using the
`NODE_CLUSTER_SCHED_POLICY` environment variable. Valid values are `"rr"`
and `"none"`.

### cluster.settings

* {Object}
  * `execArgv` {Array} list of string arguments passed to the Node.js
    executable. (Default=`process.execArgv`)
  * `exec` {String} file path to worker file.  (Default=`process.argv[1]`)
  * `args` {Array} string arguments passed to worker.
    (Default=`process.argv.slice(2)`)
  * `silent` {Boolean} whether or not to send output to parent's stdio.
    (Default=`false`)
  * `uid` {Number} Sets the user identity of the process. (See setuid(2).)
  * `gid` {Number} Sets the group identity of the process. (See setgid(2).)

The `cluster.settings` property contains the default spawn settings for all
workers. Its value is set for the first time after calling either
`cluster.setupMaster()` or `cluster.fork()` (whichever comes first).

While the object is not read-only, it's properties must only be modified
through use of the `cluster.setupMaster()` method.

### cluster.setupMaster([settings])

* `settings` {Object}
  * `exec` {String} file path to worker file.  (Default=`process.argv[1]`)
  * `args` {Array} string arguments passed to worker.
    (Default=`process.argv.slice(2)`)
  * `silent` {Boolean} whether or not to send output to parent's stdio.
    (Default=`false`)

The `cluster.setupMaster()` method is used to modify the default spawn settings
for each fork. Once called, the settings will be present in `cluster.settings`.

Note that:

* any settings changes only affect future calls to `.fork()` and have no
  effect on workers that are already running
* The *only* attribute of a worker that cannot be set via `.setupMaster()` is
  the `env` passed to `.fork()`
* the defaults above apply to the first call only, the defaults for later
  calls is the current value at the time of `cluster.setupMaster()` is called

Example:

```js
const cluster = require('cluster');
cluster.setupMaster({
  exec: 'worker.js',
  args: ['--use', 'https'],
  silent: true
});
cluster.fork(); // https worker
cluster.setupMaster({
  args: ['--use', 'http']
});
cluster.fork(); // http worker
```

The `cluster.setupMaster()` method can only be called from the master process.

### cluster.worker

* {Object}

A reference to the current worker object. Not available in the master process.

```js
const cluster = require('cluster');

if (cluster.isMaster) {
  console.log('I am master');
  cluster.fork();
  cluster.fork();
} else if (cluster.isWorker) {
  console.log(`I am worker #${cluster.worker.id}`);
}
```

### cluster.workers

* {Object}

A hash that stores the active worker objects, keyed by `id` field, making it
possible to loop through all available workers.

The `cluster.workers` property is available only in the master process.

A worker is removed from cluster.workers after the worker has disconnected _and_
exited. The ordering of these events cannot be determined in advance. However,
it is guaranteed that the removal from the cluster.workers list happens
before the final `'disconnect'` or `'exit'` event is emitted.

```js
// Go through all workers
function eachWorker(callback) {
  for (var id in cluster.workers) {
    callback(cluster.workers[id]);
  }
}
eachWorker((worker) => {
  worker.send('big announcement to all workers');
});
```

The worker's unique id is the easiest way to find a worker:

```js
socket.on('data', (id) => {
  var worker = cluster.workers[id];
});
```

## Class: Worker

Instances of the `Worker` class are [`EventEmitters`] that provide information
about a worker and methods for interacting with, or controlling, the worker.

Master processes maintain a record of all spawned worker processes using the
`cluster.workers` property. Worker processes have access only to the one
`Worker` object representing that process (using the `cluster.worker` property).

### Event: 'disconnect'

Emitted by the `Worker` object when the worker process has disconnected from
the master.

```js
cluster.fork().on('disconnect', () => {
  // Worker has disconnected
});
```

### Event: 'error'

Emitted by the `Worker` object when an error has occurred. This event is the
same as that provided by [`child_process.fork()`][].

Because the `cluster` API is built on top of the `child_process` module and
worker processes are spawned using [`child_process.fork()`][], it is also
possible to use `process.on('error')` to listen to and handle `'error'` events.

### Event: 'exit'

* `code` {Number} the exit code if the child exited on its own.
* `signal` {String} the signal by which the child process was terminated.

Emitted by the `Worker` object when the worker process exits.

```js
const worker = cluster.fork();
worker.on('exit', (code, signal) => {
  if( signal ) {
    console.log(`worker was killed by signal: ${signal}`);
  } else if( code !== 0 ) {
    console.log(`worker exited with error code: ${code}`);
  } else {
    console.log('worker success!');
  }
});
```

### Event: 'listening'

* `address` {Object}

Emitted only within the master process whenever the specific worker process
is listening on a server port. The callback function is passed a single
`address` object that includes the `address`, `port`, and `addressType`
properties.

```js
cluster.fork().on('listening', (address) => {
  // Worker is listening
});
```

### Event: 'message'

* `message` {Object}

Because the `cluster` module is built on top of the `child_process` module, and
because workers are launched using [`child_process.fork()`][], it is possible to
pass messages between the master and worker processes. The `'message'` event
is triggered on the `Worker` object whenever a message is received.

Within the worker process, you can also use `process.on('message')` to receive
messages sent from the master process.

For example, following is a cluster that keeps a count of the number of requests
in the master process using the message system:

```js
const cluster = require('cluster');
const http = require('http');

if (cluster.isMaster) {

  // Keep track of http requests
  var numReqs = 0;
  setInterval(() => {
    console.log('numReqs =', numReqs);
  }, 1000);

  // Count requests
  function messageHandler(msg) {
    if (msg.cmd && msg.cmd == 'notifyRequest') {
      numReqs += 1;
    }
  }

  // Start workers and listen for messages containing notifyRequest
  const numCPUs = require('os').cpus().length;
  for (var i = 0; i < numCPUs; i++) {
    cluster.fork();
  }

  Object.keys(cluster.workers).forEach((id) => {
    cluster.workers[id].on('message', messageHandler);
  });

} else {

  // Worker processes have a http server.
  http.Server((req, res) => {
    res.writeHead(200);
    res.end('hello world\n');

    // notify master about the request
    process.send({ cmd: 'notifyRequest' });
  }).listen(8000);
}
```

### Event: 'online'

Triggered only within the master process when a specific worker is online
and ready to receive messages.

```js
cluster.fork().on('online', () => {
  // Worker is online
});
```

### worker.disconnect()

When called from within a worker process, the `worker.disconnect()` method
will close all listening servers, wait for the `'close'` event on those servers
to be triggered, and will then disconnect the IPC channel with the master
process.

When called on the master process, an internal control message is sent to
the worker instructing it to call the `worker.disconnect()` message itself.

When called, the `worker.suicide` flag will be set to `true`.

Note that after a server is closed, new connections will no longer be accepted
by that worker, but connections may be accepted by any other listening worker.
Existing connections will be allowed to close as usual. When no more
connections exist (see [`server.close()`][]) the IPC channel to the worker will
close allowing it to exit gracefully.

Client connections, on the other hand, are not automatically closed by workers,
and disconnect does not wait for client connections to close before exiting.

Note that in a worker process, the `worker.disconnect()` and
`process.disconnect()` methods are *not* equivalent to one another
(`process.disconnect()` simply disconnects the IPC channel, while
`worker.disconnect()` closes all servers then disconnects the IPC channel).
Worker processes should always use the `worker.disconnect()` method.

Because long living server connections may block workers from disconnecting, it
may be useful to have the master process send a shutdown message to the
workers, so that application specific actions may be taken to close them.
It also may be useful to implement a timeout, killing a worker if
the `'disconnect'` event has not been emitted after some time.

```js
if (cluster.isMaster) {
  var worker = cluster.fork();
  var timeout;

  worker.on('listening', (address) => {
    worker.send('shutdown');
    worker.disconnect();
    timeout = setTimeout(() => {
      worker.kill();
    }, 2000);
  });

  worker.on('disconnect', () => {
    clearTimeout(timeout);
  });

} else if (cluster.isWorker) {
  const net = require('net');
  var server = net.createServer((socket) => {
    // connections never end
  });

  server.listen(8000);

  process.on('message', (msg) => {
    if(msg === 'shutdown') {
      // initiate graceful close of any connections to server
    }
  });
}
```

### worker.id

* {Number}

Returns the unique identifier of the worker process. While a worker is alive,
this is the key used to index it in `cluster.workers`.

### worker.isConnected()

Returns `true` if the worker is connected to its master via its IPC
channel, `false` otherwise. A worker is connected to its master after it has
been created. It is disconnected after the `'disconnect'` event is emitted.

### worker.isDead()

Returns `true` if the worker's process has terminated (either
because of exiting or being signaled). Otherwise, it returns `false`.

### worker.kill([signal='SIGTERM'])

* `signal` {String} Name of the kill signal to send to the worker
  process.

This function will, by default, signal the worker process to terminate. In the
master process, this is done by first disconnecting the worker process, then
sending the `signal` (which defaults to `SIGTERM`). Within the worker, the IPC
channel is disconnected and the process exits with code `0`.

Note that once the `worker.kill()` is called, all open server connections
will be closed and the worker process will normally exit before the `signal`
can be delivered. Sending the `signal` is only a backup in case the worker
process fails to exit on its own and code should not be written to expect the
signal to be delivered. Applications that wish to send `signals` reliably
should use `process.kill()` instead.

When called, the `worker.suicide` property will be set to `true`.

This method is aliased as `worker.destroy()` for backwards compatibility.

Note that in a worker process, the `worker.kill()` and `process.kill()` methods
are *not* equivalent to one another.

### worker.process

* {ChildProcess}

Within the master process, `worker.process` returns the [`ChildProcess`][]
object returned by [`child_process.fork()`][] when the worker was spawned.

Within the worker process, `worker.process` returns the global `process`.

Note that a worker will exit with exit code `0` and `worker.suicide` set to
`false` if `process.disconnect()` is called.

For instance:

```js
const cluster = require('cluster');

if (cluster.isMaster) {  
  var worker = cluster.fork();
  worker.on('exit', (code,sig)=>{
    console.log(
      `${worker.id} exited: ${code}, ${sig}, ${worker.suicide}`);
      // Prints: 1 exited: 0, null, false
  });
} else {
  process.disconnect();
}
```

In contrast:

```js
const cluster = require('cluster');

if (cluster.isMaster) {  
  var worker = cluster.fork();
  worker.on('exit', (code,sig)=>{
    console.log(
      `${worker.id} exited: ${code}, ${sig}, ${worker.suicide}`);
      // Prints: 1 exited: 0, null, true
  });
} else {
  cluster.worker.disconnect();
}
```

### worker.send(message[, sendHandle][, callback])

* `message` {Object}
* `sendHandle` {Handle}
* `callback` {Function}
* Return: Boolean

Within the master process, `worker.send` sends a message to worker. It is
identical to [`ChildProcess.send()`][].

Within the worker process, `worker.send` sends a message to the master. It is
identical to `process.send()`.

This example will echo back all messages from the master:

```js
if (cluster.isMaster) {
  var worker = cluster.fork();
  worker.send('hi there');

} else if (cluster.isWorker) {
  process.on('message', (msg) => {
    process.send(msg);
  });
}
```

### worker.suicide

* {Boolean}

Returns `true` if either the `worker.kill()` or `worker.disconnect()` methods
have been called, until then it is `undefined`.

The `worker.suicide` property makes is possible to distinguish between
voluntary and accidental exit of a worker. The master can choose whether to
respawn a worker based on this value.

```js
cluster.on('exit', (worker, code, signal) => {
  if (worker.suicide === true) {
    console.log('The worker meant to exit. No need to worry.').
  }
});

// kill worker
worker.kill();
```

[`child_process.fork()`]: child_process.html#child_process_child_process_fork_modulepath_args_options
[`ChildProcess.send()`]: child_process.html#child_process_child_send_message_sendhandle_callback
[`disconnect`]: child_process.html#child_process_child_disconnect
[`kill`]: process.html#process_process_kill_pid_signal
[`server.close()`]: net.html#net_event_close
[`ChildProcess`]: child_process.html#childprocess
[child_process event: 'exit']: child_process.html#child_process_event_exit
[child_process event: 'message']: child_process.html#child_process_event_message
