'use strict';
var spawn = require('child_process').spawn;
var cluster = require('cluster');
var http = require('http');

var options = {
  mode: 'master',
  host: '127.0.0.1',
  port: 22344,
  path: '/',
  servers: 1,
  clients: 1,
  clientConcurrentRequests: 2
};

for (var i = 2; i < process.argv.length; ++i) {
  var args = process.argv[i].split('=', 2);
  var key = args[0];
  var val = args[1];
  options[key] = val;
}

switch (options.mode) {
  case 'master': startMaster(); break;
  case 'server': startServer(); break;
  case 'client': startClient(); break;
  default: throw new Error('Bad mode: ' + options.mode);
}

process.title = 'http_bench[' + options.mode + ']';

// monkey-patch the log functions so they include name + pid
console.log = patch(console.log);
console.trace = patch(console.trace);
console.error = patch(console.error);

function patch(fun) {
  var prefix = process.title + '[' + process.pid + '] ';
  return function() {
    var args = Array.prototype.slice.call(arguments);
    args[0] = prefix + args[0];
    return fun.apply(console, args);
  };
}

function startMaster() {
  if (!cluster.isMaster) return startServer();

  var forkCount = 0;

  cluster.on('online', function() {
    forkCount = forkCount + 1;
    if (forkCount === ~~options.servers) {
      var args = [
        __filename,
        'mode=client',
        'clientConcurrentRequests=' + options.clientConcurrentRequests
      ];
      for (var i = ~~options.clients; i > 0; --i) {
        var cp = spawn(process.execPath, args);
        cp.stdout.pipe(process.stdout);
        cp.stderr.pipe(process.stderr);
      }
    }
  });

  for (var i = ~~options.servers; i > 0; --i) cluster.fork();
}

function startServer() {
  http.createServer(onRequest).listen(options.port, options.host);

  var body = Array(1024).join('x');
  var headers = {'Content-Length': '' + body.length};

  function onRequest(req, res) {
    req.on('error', onError);
    res.on('error', onError);
    res.writeHead(200, headers);
    res.end(body);
  }

  function onError(err) {
    console.error(err.stack);
  }
}

function startClient() {
  // send off a bunch of concurrent requests
  for (var i = ~~options.clientConcurrentRequests; i > 0; --i) {
    sendRequest();
  }

  function sendRequest() {
    var req = http.request(options, onConnection);
    req.on('error', onError);
    req.end();
  }

  // add a little back-off to prevent EADDRNOTAVAIL errors, it's pretty easy
  // to exhaust the available port range
  function relaxedSendRequest() {
    setTimeout(sendRequest, 1);
  }

  function onConnection(res) {
    res.on('error', onError);
    res.on('data', onData);
    res.on('end', relaxedSendRequest);
  }

  function onError(err) {
    console.error(err.stack);
    relaxedSendRequest();
  }

  function onData(data) {
    // this space intentionally left blank
  }
}
