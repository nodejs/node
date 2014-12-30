"use strict";
var path = require('path');
var http = require('http');
var fs = require('fs');
var spawn = require('child_process').spawn;
var common = require('../common.js')
var test = require('../../test/common.js')
var pep = path.dirname(process.argv[1])+'/chunky_http_client.js';
var PIPE = test.PIPE;

var server;

fs.unlinkSync(PIPE);
server = http.createServer(function(req, res) {
  res.writeHead(200, { 'content-type': 'text/plain',
                       'content-length': '2' });
  res.end('ok');
});

server.on('error', function(err) {
  console.log("Error:");
  console.log(err);
});

try {
  server.listen(PIPE, 'localhost');
} catch(e) {
  console.log("oops!");
  console.log(e);
}

var child = spawn(process.execPath, [pep], { });
child.on('error', function(err) {
  console.log('spawn error.');
  console.log(err);
});

child.stdout.on('data', function (data) {
  process.stdout.write(data);
});

child.on('exit', function (exitCode) {
  console.log("Child exited with code: " + exitCode);
  process.exit(0);
});
