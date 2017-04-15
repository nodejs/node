'use strict';

var path = require('path');
var http = require('http');
var fs = require('fs');
var fork = require('child_process').fork;
var common = require('../common.js');
var test = require('../../test/common.js');
var pep = path.dirname(process.argv[1]) + '/_chunky_http_client.js';
var PIPE = test.PIPE;

try {
  fs.accessSync(test.tmpDir, fs.F_OK);
} catch (e) {
  fs.mkdirSync(test.tmpDir);
}

var server;
try {
  fs.unlinkSync(PIPE);
} catch (e) { /* ignore */ }

server = http.createServer(function(req, res) {
  var headers = {
    'content-type': 'text/plain',
    'content-length': '2'
  };
  res.writeHead(200, headers);
  res.end('ok');
});

server.on('error', function(err) {
  throw new Error('server error: ' + err);
});

server.listen(PIPE);

var child = fork(pep, process.argv.slice(2));
child.on('message', common.sendResult);
child.on('close', function() {
  server.close();
});
