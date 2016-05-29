'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var http = require('http');
var path = require('path');
var cp = require('child_process');

common.refreshTmpDir();

var filename = path.join(common.tmpDir || '/tmp', 'big');
var clientReqComplete = false;
var count = 0;

var server = http.createServer(function(req, res) {
  var timeoutId;
  assert.equal('POST', req.method);
  req.pause();

  setTimeout(function() {
    req.resume();
  }, 1000);

  req.on('data', function(chunk) {
    count += chunk.length;
  });

  req.on('end', function() {
    if (timeoutId) {
      clearTimeout(timeoutId);
    }
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end();
  });
});
server.listen(0);

server.on('listening', function() {
  var cmd = common.ddCommand(filename, 10240);

  cp.exec(cmd, function(err, stdout, stderr) {
    if (err) throw err;
    makeRequest();
  });
});

function makeRequest() {
  var req = http.request({
    port: server.address().port,
    path: '/',
    method: 'POST'
  });

  var s = fs.ReadStream(filename);
  s.pipe(req);
  s.on('close', function(err) {
    if (err) throw err;
    clientReqComplete = true;
  });

  req.on('response', function(res) {
    res.resume();
    res.on('end', function() {
      server.close();
    });
  });
}

process.on('exit', function() {
  assert.equal(1024 * 10240, count);
  assert.ok(clientReqComplete);
});
