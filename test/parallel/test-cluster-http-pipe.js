'use strict';

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const http = require('http');

if (common.isWindows) {
  common.skip('It is not possible to send pipe handles over ' +
              'the IPC pipe on Windows');
  return;
}

if (cluster.isMaster) {
  common.refreshTmpDir();
  var worker = cluster.fork();
  worker.on('message', common.mustCall(function(msg) {
    assert.equal(msg, 'DONE');
  }));
  worker.on('exit', function() {
    process.exit();
  });
  return;
}

http.createServer(function(req, res) {
  assert.equal(req.connection.remoteAddress, undefined);
  assert.equal(req.connection.localAddress, undefined); // TODO common.PIPE?
  res.writeHead(200);
  res.end('OK');
}).listen(common.PIPE, function() {
  http.get({ socketPath: common.PIPE, path: '/' }, function(res) {
    res.resume();
    res.on('end', function(err) {
      if (err) throw err;
      process.send('DONE');
      process.exit();
    });
  });
});
