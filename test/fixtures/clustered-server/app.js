var http = require('http');
var cluster = require('cluster');
var common = require('../../common');

function handleRequest(request, response) {
  response.end('hello world\n');
}

var NUMBER_OF_WORKERS = 2;
var workersOnline = 0;

if (cluster.isMaster) {
  cluster.on('online', function() {
    if (++workersOnline === NUMBER_OF_WORKERS) {
      console.error('all workers are running');
    }
  });

  process.on('message', function(msg) {
    if (msg.type === 'getpids') {
      var pids = [];
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
  var server = http.createServer(handleRequest);
  server.listen(common.PORT+1000);
}
