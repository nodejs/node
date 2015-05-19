'use strict';
// It is not possible to send pipe handles over the IPC pipe on Windows.
if (process.platform === 'win32') {
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var http = require('http');

if (cluster.isMaster) {
  var ok = false;
  var worker = cluster.fork();
  worker.on('message', function(msg) {
    assert.equal(msg, 'DONE');
    ok = true;
  });
  worker.on('exit', function() {
    process.exit();
  });
  process.on('exit', function() {
    assert(ok);
  });
  return;
}

http.createServer(function(req, res) {
  assert.equal(req.connection.remoteAddress, undefined);
  assert.equal(req.connection.localAddress, undefined); // TODO common.PIPE?
  res.writeHead(200);
  res.end('OK');
}).listen(common.PIPE, function() {
  var self = this;
  http.get({ socketPath: common.PIPE, path: '/' }, function(res) {
    res.resume();
    res.on('end', function(err) {
      if (err) throw err;
      process.send('DONE');
      process.exit();
    });
  });
});
