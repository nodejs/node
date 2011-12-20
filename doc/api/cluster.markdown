## Cluster

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


### cluster.isMaster

This boolean flag is true if the process is a master. This is determined
by the `process.env.NODE_UNIQUE_ID`. If `process.env.NODE_UNIQUE_ID` is
undefined `isMaster` is `true`.

### cluster.isWorker

This boolean flag is true if the process is a worker forked from a master.
If the `process.env.NODE_UNIQUE_ID` is set to a value different efined
`isWorker` is `true`.

### Event: 'fork'

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

### Event: 'online'

After forking a new worker, the worker should respond with a online message.
When the master receives a online message it will emit such event.
The difference between 'fork' and 'online' is that fork is emitted when the
master tries to fork a worker, and 'online' is emitted when the worker is being
executed.

    cluster.on('online', function (worker) {
      console.log("Yay, the worker responded after it was forked");
    });

### Event: 'listening'

When calling `listen()` from a worker, a 'listening' event is automatically assigned
to the server instance. When the server is listening a message is send to the master
where the 'listening' event is emitted.

    cluster.on('listening', function (worker) {
      console.log("We are now connected");
    });

### Event: 'death'

When any of the workers die the cluster module will emit the 'death' event.
This can be used to restart the worker by calling `fork()` again.

    cluster.on('death', function(worker) {
      console.log('worker ' + worker.pid + ' died. restart...');
      cluster.fork();
    });

### cluster.fork([env])

Spawn a new worker process. This can only be called from the master process.
The function takes an optional `env` object. The properties in this object
will be added to the process environment in the worker.

### cluster.workers

In the cluster all living worker objects are stored in this object by there
`uniqueID` as the key. This makes it easy to loop thouge all liveing workers.

    // Go througe all workers
    function eachWorker(callback) {
      for (var uniqueID in cluster.workers) {
        callback(cluster.workers[uniqueID]);
      }
    }
    eachWorker(function (worker) {
      worker.send('big announcement to all workers');
    });

Should you wich to reference a worker over a communication channel this unsing
there `uniqueID` this is also the easies way to find the worker.

    socket.on('data', function (uniqueID) {
      var worker = cluster.workers[uniqueID];
    });

## Worker

This object contains all public information and method about a worker.
In the master it can be obtainedusing `cluster.workers`. In a worker
it can be obtained ained using `cluster.worker`.

### Worker.uniqueID

Each new worker is given its own unique id, this id i stored in the `uniqueID`.

### Worker.process

All workers are created using `child_process.fork()`, the returned object from this
function is stored in process.

### Worker.send(message, [sendHandle])

This function is equal to the send methods provided by `child_process.fork()`.
In the master you should use this function to send a message to a specific worker.
However in a worker you can also use `process.send(message)`, since this is the same
function.

This example will echo back all messages from the master:

    if (cluster.isMaster) {
      var worker = cluster.fork();
      worker.send('hi there');

    } else if (cluster.isWorker) {
      process.on('message', function (msg) {
        process.send(msg);
      });
    }

### Worker.destroy()

This function will kill the worker, and inform the master to not spawn a new worker.
To know the difference between suicide and accidently death a suicide boolean is set to true.

    cluster.on('death', function (worker) {
      if (worker.suicide === true) {
        console.log('Oh, it was just suicide' – no need to worry').
      }
    });

    // destroy worker
    worker.destroy();

### Worker.suicide

This property is a boolean. It is set when a worker dies, until then it is `undefined`.
It is true if the worker was killed using the `.destroy()` method, and false otherwise.

### Event: message

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

### Event: online

Same as the `cluster.on('online')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('online', function (worker) {
      // Worker is online
    };

### Event: listening

Same as the `cluster.on('listening')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('listening', function (worker) {
      // Worker is listening
    };

### Event: death

Same as the `cluster.on('death')` event, but emits only when the state change
on the specified worker.

    cluster.fork().on('death', function (worker) {
      // Worker has died
    };
