'use strict';

const http = require('http');
const cluster = require('cluster');

function handleRequest(request, response) {
  response.end('hello world\n');
}

const NUMBER_OF_WORKERS = 2;
var workersOnline = 0;

if (cluster.isMaster) {
  cluster.on('online', function() {
    if (++workersOnline === NUMBER_OF_WORKERS) {
      console.error('all workers are running');
    }
  });

  process.on('message', function(msg) {
    if (msg.type === 'getpids') {
      const pids = [];
      pids.push(process.pid);
      for (var key in cluster.workers)
        pids.push(cluster.workers[key].process.pid);
      process.send({ type: 'pids', pids: pids });
    }
  });

  for (var i = 0; i < NUMBER_OF_WORKERS; i++) {
    cluster.fork();
  }
} else {
  const server = http.createServer(handleRequest);
  server.listen(0);
}
