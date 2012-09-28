# Cluster

    Stability: 1 - Experimental

A single instance of Node runs in a single thread. To take advantage of
multi-core systems the user will sometimes want to launch a cluster of Node
processes to handle the load.

The cluster module allows you to easily create a network of processes that
all share server ports.

    var cluster = require('cluster');
    var http = require('http');
    var numCPUs = require('os').cpus().length;

    if (cluster.isMaster) {
      // Fork workers.
      for (var i = 0; i < numCPUs; i++) {
        cluster.fork();
      }

      cluster.on('exit', function(worker, code, signal) {
        console.log('worker ' + worker.process.pid + ' died');
      });
    } else {
      // Workers can share any TCP connection
      // In this case its a HTTP server
      http.createServer(function(req, res) {
        res.writeHead(200);
        res.end("hello world\n");
      }).listen(8000);
    }

Running node will now share port 8000 between the workers:

    % node server.js
    Worker 2438 online
    Worker 2437 online


This feature was introduced recently, and may change in future versions.
Please try it out and provide feedback.

Also note that, on Windows, it is not yet possible to set up a named pipe
server in a worker.

## How It Works

<!--type=misc-->

The worker processes are spawned using the `child_process.fork` method,
so that they can communicate with the parent via IPC and pass server
handles back and forth.

When you call `server.listen(...)` in a worker, it serializes the
arguments and passes the request to the master process.  If the master
process already has a listening server matching the worker's
requirements, then it passes the handle to the worker.  If it does not
already have a listening server matching that requirement, then it will
create one, and pass the handle to the child.

This causes potentially surprising behavior in three edge cases:

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

When multiple processes are all `accept()`ing on the same underlying
resource, the operating system load-balances across them very
efficiently.  There is no routing logic in Node.js, or in your program,
and no shared state between the workers.  Therefore, it is important to
design your program such that it does not rely too heavily on in-memory
data objects for things like sessions and login.

Because workers are all separate processes, they can be killed or
re-spawned depending on your program's needs, without affecting other
workers.  As long as there are some workers still alive, the server will
continue to accept connections.  Node does not automatically manage the
number of workers for you, however.  It is your responsibility to manage
the worker pool for your application's needs.

## cluster.settings

* {Object}
  * `exec` {String} file path to worker file.  (Default=`__filename`)
  * `args` {Array} string arguments passed to worker.
    (Default=`process.argv.slice(2)`)
  * `silent` {Boolean} whether or not to send output to parent's stdio.
    (Default=`false`)

All settings set by the `.setupMaster` is stored in this settings object.
This object is not supposed to be change or set manually, by you.

## cluster.isMaster

* {Boolean}

True if the process is a master. This is determined
by the `process.env.NODE_UNIQUE_ID`. If `process.env.NODE_UNIQUE_ID` is
undefined, then `isMaster` is `true`.

## cluster.isWorker

* {Boolean}

This boolean flag is true if the process is a worker forked from a master.
If the `process.env.NODE_UNIQUE_ID` is set to a value, then
`isWorker` is `true`.

## Event: 'fork'

* `worker` {Worker object}

When a new worker is forked the cluster module will emit a 'fork' event.
This can be used to log worker activity, and create you own timeout.

    var timeouts = [];
    function errorMsg() {
      console.error("Something must be wrong with the connection ...");
    }

    cluster.on('fork', function(worker) {
      timeouts[worker.id] = setTimeout(errorMsg, 2000);
    });
    cluster.on('listening', function(worker, address) {
      clearTimeout(timeouts[worker.id]);
    });
    cluster.on('exit', function(worker, code, signal) {
      clearTimeout(timeouts[worker.id]);
      errorMsg();
    });

## Event: 'online'

* `worker` {Worker object}

After forking a new worker, the worker should respond with a online message.
When the master receives a online message it will emit such event.
The difference between 'fork' and 'online' is that fork is emitted when the
master tries to fork a worker, and 'online' is emitted when the worker is
being executed.

    cluster.on('online', function(worker) {
      console.log("Yay, the worker responded after it was forked");
    });

## Event: 'listening'

* `worker` {Worker object}
* `address` {Object}

When calling `listen()` from a worker, a 'listening' event is automatically assigned
to the server instance. When the server is listening a message is send to the master
where the 'listening' event is emitted.

The event handler is executed with two arguments, the `worker` contains the worker
object and the `address` object contains the following connection properties:
`address`, `port` and `addressType`. This is very useful if the worker is listening
on more than one address.

    cluster.on('listening', function(worker, address) {
      console.log("A worker is now connected to " + address.address + ":" + address.port);
    });

## Event: 'disconnect'

* `worker` {Worker object}

When a workers IPC channel has disconnected this event is emitted. This will happen
when the worker dies, usually after calling `.destroy()`.

When calling `.disconnect()`, there may be a delay between the
`disconnect` and `exit` events.  This event can be used to detect if
the process is stuck in a cleanup or if there are long-living
connections.

    cluster.on('disconnect', function(worker) {
      console.log('The worker #' + worker.id + ' has disconnected');
    });

## Event: 'exit'

* `worker` {Worker object}
* `code` {Number} the exit code, if it exited normally.
* `signal` {String} the name of the signal (eg. `'SIGHUP'`) that caused
  the process to be killed.

When any of the workers die the cluster module will emit the 'exit' event.
This can be used to restart the worker by calling `fork()` again.

    cluster.on('exit', function(worker, code, signal) {
      var exitCode = worker.process.exitCode;
      console.log('worker ' + worker.process.pid + ' died ('+exitCode+'). restarting...');
      cluster.fork();
    });

## Event: 'setup'

* `worker` {Worker object}

When the `.setupMaster()` function has been executed this event emits.
If `.setupMaster()` was not executed before `fork()` this function will
call `.setupMaster()` with no arguments.

## cluster.setupMaster([settings])

* `settings` {Object}
  * `exec` {String} file path to worker file.  (Default=`__filename`)
  * `args` {Array} string arguments passed to worker.
    (Default=`process.argv.slice(2)`)
  * `silent` {Boolean} whether or not to send output to parent's stdio.
    (Default=`false`)

`setupMaster` is used to change the default 'fork' behavior. The new settings
are effective immediately and permanently, they cannot be changed later on.

Example:

    var cluster = require("cluster");
    cluster.setupMaster({
      exec : "worker.js",
      args : ["--use", "https"],
      silent : true
    });
    cluster.fork();

## cluster.fork([env])

* `env` {Object} Key/value pairs to add to child process environment.
* return {Worker object}

Spawn a new worker process. This can only be called from the master process.

## cluster.disconnect([callback])

* `callback` {Function} called when all workers are disconnected and handlers are closed

When calling this method, all workers will commit a graceful suicide. When they are
disconnected all internal handlers will be closed, allowing the master process to
die graceful if no other event is waiting.

The method takes an optional callback argument which will be called when finished.

## cluster.worker

* {Object}

A reference to the current worker object. Not available in the master process.

    var cluster = require('cluster');

    if (cluster.isMaster) {
      console.log('I am master');
      cluster.fork();
      cluster.fork();
    } else if (cluster.isWorker) {
      console.log('I am worker #' + cluster.worker.id);
    }

## cluster.workers

* {Object}

A hash that stores the active worker objects, keyed by `id` field. Makes it
easy to loop through all the workers. It is only available in the master
process.

    // Go through all workers
    function eachWorker(callback) {
      for (var id in cluster.workers) {
        callback(cluster.workers[id]);
      }
    }
    eachWorker(function(worker) {
      worker.send('big announcement to all workers');
    });

Should you wish to reference a worker over a communication channel, using
the worker's unique id is the easiest way to find the worker.

    socket.on('data', function(id) {
      var worker = cluster.workers[id];
    });

## Class: Worker

A Worker object contains all public information and method about a worker.
In the master it can be obtained using `cluster.workers`. In a worker
it can be obtained using `cluster.worker`.

### worker.id

* {String}

Each new worker is given its own unique id, this id is stored in the
`id`.

While a worker is alive, this is the key that indexes it in
cluster.workers

### worker.process

* {ChildProcess object}

All workers are created using `child_process.fork()`, the returned object
from this function is stored in process.

See: [Child Process module](child_process.html)

### worker.suicide

* {Boolean}

This property is a boolean. It is set when a worker dies after calling `.destroy()`
or immediately after calling the `.disconnect()` method. Until then it is `undefined`.

### worker.send(message, [sendHandle])

* `message` {Object}
* `sendHandle` {Handle object}

This function is equal to the send methods provided by
`child_process.fork()`.  In the master you should use this function to
send a message to a specific worker.  However in a worker you can also use
`process.send(message)`, since this is the same function.

This example will echo back all messages from the master:

    if (cluster.isMaster) {
      var worker = cluster.fork();
      worker.send('hi there');

    } else if (cluster.isWorker) {
      process.on('message', function(msg) {
        process.send(msg);
      });
    }

### worker.destroy()

This function will kill the worker, and inform the master to not spawn a
new worker.  The boolean `suicide` lets you distinguish between voluntary
and accidental exit.

    cluster.on('exit', function(worker, code, signal) {
      if (worker.suicide === true) {
        console.log('Oh, it was just suicide\' – no need to worry').
      }
    });

    // destroy worker
    worker.destroy();


### worker.disconnect()

When calling this function the worker will no longer accept new connections, but
they will be handled by any other listening worker. Existing connection will be
allowed to exit as usual. When no more connections exist, the IPC channel to the worker
will close allowing it to die graceful. When the IPC channel is closed the `disconnect`
event will emit, this is then followed by the `exit` event, there is emitted when
the worker finally die.

Because there might be long living connections, it is useful to implement a timeout.
This example ask the worker to disconnect and after 2 seconds it will destroy the
server. An alternative would be to execute `worker.destroy()` after 2 seconds, but
that would normally not allow the worker to do any cleanup if needed.

    if (cluster.isMaster) {
      var worker = cluster.fork();
      var timeout;

      worker.on('listening', function(address) {
        worker.disconnect();
        timeout = setTimeout(function() {
          worker.send('force kill');
        }, 2000);
      });

      worker.on('disconnect', function() {
        clearTimeout(timeout);
      });

    } else if (cluster.isWorker) {
      var net = require('net');
      var server = net.createServer(function(socket) {
        // connection never end
      });

      server.listen(8000);

      server.on('close', function() {
        // cleanup
      });

      process.on('message', function(msg) {
        if (msg === 'force kill') {
          server.destroy();
        }
      });
    }

### Event: 'message'

* `message` {Object}

This event is the same as the one provided by `child_process.fork()`.
In the master you should use this event, however in a worker you can also use
`process.on('message')`

As an example, here is a cluster that keeps count of the number of requests
in the master process using the message system:

    var cluster = require('cluster');
    var http = require('http');

    if (cluster.isMaster) {

      // Keep track of http requests
      var numReqs = 0;
      setInterval(function() {
        console.log("numReqs =", numReqs);
      }, 1000);

      // Count requestes
      function messageHandler(msg) {
        if (msg.cmd && msg.cmd == 'notifyRequest') {
          numReqs += 1;
        }
      }

      // Start workers and listen for messages containing notifyRequest
      var numCPUs = require('os').cpus().length;
      for (var i = 0; i < numCPUs; i++) {
        cluster.fork();
      }

      Object.keys(cluster.workers).forEach(function(id) {
        cluster.workers[id].on('message', messageHandler);
      });

    } else {

      // Worker processes have a http server.
      http.Server(function(req, res) {
        res.writeHead(200);
        res.end("hello world\n");

        // notify master about the request
        process.send({ cmd: 'notifyRequest' });
      }).listen(8000);
    }

### Event: 'online'

Same as the `cluster.on('online')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('online', function() {
      // Worker is online
    };

### Event: 'listening'

* `address` {Object}

Same as the `cluster.on('listening')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('listening', function(address) {
      // Worker is listening
    };

### Event: 'disconnect'

Same as the `cluster.on('disconnect')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('disconnect', function() {
      // Worker has disconnected
    };

### Event: 'exit'

* `code` {Number} the exit code, if it exited normally.
* `signal` {String} the name of the signal (eg. `'SIGHUP'`) that caused
  the process to be killed.

Emitted by the individual worker instance, when the underlying child process
is terminated.  See [child_process event: 'exit'](child_process.html#child_process_event_exit).

    var worker = cluster.fork();
    worker.on('exit', function(code, signal) {
      if( signal ) {
        console.log("worker was killed by signal: "+signal);
      } else if( code !== 0 ) {
        console.log("worker exited with error code: "+code);
      } else {
        console.log("worker success!");
      }
    };
