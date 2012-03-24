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

      cluster.on('death', function(worker) {
        console.log('worker ' + worker.pid + ' died');
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
    var errorMsg = function () {
      console.error("Something must be wrong with the connection ...");
    });

    cluster.on('fork', function (worker) {
      timeouts[worker.uniqueID] = setTimeout(errorMsg, 2000);
    });
    cluster.on('listening', function (worker) {
      clearTimeout(timeouts[worker.uniqueID]);
    });
    cluster.on('death', function (worker) {
      clearTimeout(timeouts[worker.uniqueID]);
      errorMsg();
    });

## Event: 'online'

* `worker` {Worker object}

After forking a new worker, the worker should respond with a online message.
When the master receives a online message it will emit such event.
The difference between 'fork' and 'online' is that fork is emitted when the
master tries to fork a worker, and 'online' is emitted when the worker is
being executed.

    cluster.on('online', function (worker) {
      console.log("Yay, the worker responded after it was forked");
    });

## Event: 'listening'

* `worker` {Worker object}

When calling `listen()` from a worker, a 'listening' event is automatically assigned
to the server instance. When the server is listening a message is send to the master
where the 'listening' event is emitted.

    cluster.on('listening', function (worker) {
      console.log("We are now connected");
    });

## Event: 'disconnect'

* `worker` {Worker object}

When a workers IPC channel has disconnected this event is emitted. This will happen
when the worker dies, usually after calling `.destroy()`.

When calling `.disconnect()`, there may be a delay between the
`disconnect` and `death` events.  This event can be used to detect if
the process is stuck in a cleanup or if there are long-living
connections.

    cluster.on('disconnect', function(worker) {
      console.log('The worker #' + worker.uniqueID + ' has disconnected');
    });

## Event: 'death'

* `worker` {Worker object}

When any of the workers die the cluster module will emit the 'death' event.
This can be used to restart the worker by calling `fork()` again.

    cluster.on('death', function(worker) {
      console.log('worker ' + worker.pid + ' died. restart...');
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

The `setupMaster` is used to change the default 'fork' behavior. It takes
one option object argument.

Example:

    var cluster = require("cluster");
    cluster.setupMaster({
      exec : "worker.js",
      args : ["--use", "https"],
      silent : true
    });
    cluster.autoFork();

## cluster.fork([env])

* `env` {Object} Key/value pairs to add to child process environment.
* return {Worker object}

Spawn a new worker process. This can only be called from the master process.

## cluster.settings

* {Object}
  * `exec` {String} file path to worker file.  Default: `__filename`
  * `args` {Array} string arguments passed to worker.
    (Default=`process.argv.slice(2)`)
  * `silent` {Boolean} whether or not to send output to parent's stdio.
    (Default=`false`)

All settings set by the `.setupMaster` is stored in this settings object.
This object is not supposed to be change or set manually.

## cluster.disconnect([callback])

* `callback` {Function} called when all workers are disconnected and handlers are closed

When calling this method, all workers will commit a graceful suicide. When they are
disconnected all internal handlers will be closed, allowing the master process to
die graceful if no other event is waiting.

The method takes an optional callback argument which will be called when finished.

## cluster.workers

* {Object}

In the cluster all living worker objects are stored in this object by there
`uniqueID` as the key. This makes it easy to loop through all living workers.

    // Go through all workers
    function eachWorker(callback) {
      for (var uniqueID in cluster.workers) {
        callback(cluster.workers[uniqueID]);
      }
    }
    eachWorker(function (worker) {
      worker.send('big announcement to all workers');
    });

Should you wish to reference a worker over a communication channel, using
the worker's uniqueID is the easiest way to find the worker.

    socket.on('data', function (uniqueID) {
      var worker = cluster.workers[uniqueID];
    });

## Class: Worker

A Worker object contains all public information and method about a worker.
In the master it can be obtained using `cluster.workers`. In a worker
it can be obtained using `cluster.worker`.

### worker.uniqueID

* {String}

Each new worker is given its own unique id, this id is stored in the
`uniqueID`.

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
      process.on('message', function (msg) {
        process.send(msg);
      });
    }

### worker.destroy()

This function will kill the worker, and inform the master to not spawn a
new worker.  To know the difference between suicide and accidentally death
a suicide boolean is set to true.

    cluster.on('death', function (worker) {
      if (worker.suicide === true) {
        console.log('Oh, it was just suicide\' – no need to worry').
      }
    });

    // destroy worker
    worker.destroy();


## Worker.disconnect()

When calling this function the worker will no longer accept new connections, but
they will be handled by any other listening worker. Existing connection will be
allowed to exit as usual. When no more connections exist, the IPC channel to the worker
will close allowing it to die graceful. When the IPC channel is closed the `disconnect`
event will emit, this is then followed by the `death` event, there is emitted when
the worker finally die.

Because there might be long living connections, it is useful to implement a timeout.
This example ask the worker to disconnect and after 2 seconds it will destroy the
server. An alternative wound be to execute `worker.destroy()` after 2 seconds, but
that would normally not allow the worker to do any cleanup if needed.

    if (cluster.isMaster) {
      var worker = cluser.fork();
      var timeout;

      worker.on('listening', function () {
        worker.disconnect();
        timeout = setTimeout(function () {
          worker.send('force kill');
        }, 2000);
      });

      worker.on('disconnect', function () {
        clearTimeout(timeout);
      });

    } else if (cluster.isWorker) {
      var net = require('net');
      var server = net.createServer(function (socket) {
        // connection never end
      });

      server.listen(8000);

      server.on('close', function () {
        // cleanup
      });

      process.on('message', function (msg) {
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
      var messageHandler = function (msg) {
        if (msg.cmd && msg.cmd == 'notifyRequest') {
          numReqs += 1;
        }
      };

      // Start workers and listen for messages containing notifyRequest
      cluster.autoFork();
      Object.keys(cluster.workers).forEach(function (uniqueID) {
        cluster.workers[uniqueID].on('message', messageHandler);
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

* `worker` {Worker object}

Same as the `cluster.on('online')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('online', function (worker) {
      // Worker is online
    };

### Event: 'listening'

* `worker` {Worker object}

Same as the `cluster.on('listening')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('listening', function (worker) {
      // Worker is listening
    };

### Event: 'disconnect'

* `worker` {Worker object}

Same as the `cluster.on('disconnect')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('disconnect', function (worker) {
      // Worker has disconnected
    };

### Event: 'death'

* `worker` {Worker object}

Same as the `cluster.on('death')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('death', function (worker) {
      // Worker has died
    };
