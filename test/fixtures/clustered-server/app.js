var http = require('http');
var cluster = require('cluster');
var common = require('../../common.js');

function handleRequest(request, response) {
  response.end('hello world\n');
}

var NUMBER_OF_WORKERS = 2;
var workersOnline = 0;

if (cluster.isMaster) {
  cluster.on('online', function() {
      workersOnline++;
      if (workersOnline == NUMBER_OF_WORKERS)
        console.error('all workers are running');
    });

  for (var i = 0; i < NUMBER_OF_WORKERS; i++) {
    cluster.fork();
  }
} else {
  var server = http.createServer(handleRequest);
  server.listen(common.PORT+1000);
}
