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
  const worker = cluster.fork();
  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'DONE');
  }));
  worker.on('exit', common.mustCall(() => {}));
  return;
}

http.createServer(common.mustCall((req, res) => {
  assert.strictEqual(req.connection.remoteAddress, undefined);
  assert.strictEqual(req.connection.localAddress, undefined);
  // TODO common.PIPE?

  res.writeHead(200);
  res.end('OK');
})).listen(common.PIPE, common.mustCall(() => {
  http.get({ socketPath: common.PIPE, path: '/' }, common.mustCall((res) => {
    res.resume();
    res.on('end', common.mustCall((err) => {
      assert.ifError(err);
      process.send('DONE');
      process.exit();
    }));
  }));
}));
