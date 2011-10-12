## Cluster

A single instance of Node runs in a single thread. To take advantage of
multi-core systems the user will sometimes want to launch a cluster of Node
processes to handle the load.

By starting node with the `cluster` argument, Node will detect the number of
CPUs on the machine and start that many processes. For example suppose we
had a simple HTTP server in server.js:

    require('http').createServer(function(req, res) {
      res.writeHead(200);
      res.end('hello world\n');
    }).listen(8000);

If we start it like this

    % node cluster server.js 
    Detected 2 cpus
    Worker 2438 online
    Worker 2437 online

Node will automatically share port 8000 between the multiple instances.
