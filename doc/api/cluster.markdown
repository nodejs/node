## Cluster

A single instance of Node runs in a single thread. To take advantage of
multi-core systems the user will sometimes want to launch a cluster of Node
processes to handle the load.

The cluster module allows you to easily create a network of processes all
which share server ports.

    var cluster = require('cluster');
    var http = require('http');

    if (cluster.isMaster) {
      // Start the master process, fork workers.
      cluster.startMaster({ workers: 2 });
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

### exports.startMaster([options])

  Spawns the initial worker processes, one per CPU by default.

  The following options are supported:

  - `workerFilename`: script to execute in the worker process, defaults to
    `process.argv[1]`
  - `args`: worker program arguments, defaulting to `process.argv.slice(2)`
  - `workers`: the number of workers, defaulting to `os.cpus().length`

### exports.spawnWorker([options])

   Spawn a new worker process. This is called within `cluster.startMaster()`,
   however it is useful to implement worker resuscitation as described below
   in the "Common patterns" section.

   The `options` available are identical to `cluster.startMaster()`.

## Common patterns

## Worker resuscitation

The following is an example of how you may implement worker resuscitation,
spawning a new worker process when another exits.

    if (cluster.isMaster) {
      cluster.startMaster();
      process.on('SIGCHLD', function(){
        console.log('worker killed');
        cluster.spawnWorker();
      });
    }

