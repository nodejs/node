# Cluster

    Stability: 1 Experimental - Drastic changes in future versions

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
      // Worker processes have a http server.
      http.Server(function(req, res) {
        res.writeHead(200);
        res.end("hello world\n");
      }).listen(8000);
    }

Running node will now share port 8000 between the workers:

    % node server.js
    Worker 2438 online
    Worker 2437 online

The difference between `cluster.fork()` and `child_process.fork()` is simply
that cluster allows TCP servers to be shared between workers. `cluster.fork`
is implemented on top of `child_process.fork`. The message passing API that
is available with `child_process.fork` is available with `cluster` as well.
As an example, here is a cluster which keeps count of the number of requests
in the master process via message passing:

    var cluster = require('cluster');
    var http = require('http');
    var numReqs = 0;

    if (cluster.isMaster) {
      // Fork workers.
      for (var i = 0; i < 2; i++) {
        var worker = cluster.fork();

        worker.on('message', function(msg) {
          if (msg.cmd && msg.cmd == 'notifyRequest') {
            numReqs++;
          }
        });
      }

      setInterval(function() {
        console.log("numReqs =", numReqs);
      }, 1000);
    } else {
      // Worker processes have a http server.
      http.Server(function(req, res) {
        res.writeHead(200);
        res.end("hello world\n");
        // Send message to master process
        process.send({ cmd: 'notifyRequest' });
      }).listen(8000);
    }



## cluster.fork()

Spawn a new worker process. This can only be called from the master process.

## cluster.isMaster
## cluster.isWorker

Boolean flags to determine if the current process is a master or a worker
process in a cluster. A process `isMaster` if `process.env.NODE_WORKER_ID`
is undefined.

## Event: 'death'

When any of the workers die the cluster module will emit the 'death' event.
This can be used to restart the worker by calling `fork()` again.

    cluster.on('death', function(worker) {
      console.log('worker ' + worker.pid + ' died. restart...');
      cluster.fork();
    });

Different techniques can be used to restart the worker depending on the
application.
