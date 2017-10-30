'use strict';
const common = require('../common');
const http = require('http');
const cluster = require('cluster');
const assert = require('assert');

cluster.schedulingPolicy = cluster.SCHED_NONE;

const server = http.createServer();
if (cluster.isMaster) {
  let worker;

  server.listen(0, common.mustCall((error) => {
    assert.ifError(error);
    assert(worker);

    worker.send({ port: server.address().port });
  }));

  worker = cluster.fork();
  worker.on('exit', common.mustCall(() => {
    server.close();
  }));
} else {
  process.on('message', common.mustCall((msg) => {
    assert(msg.port);

    server.listen(msg.port);
    server.on('error', common.mustCall((e) => {
      cluster.worker.disconnect();
    }));
  }));
}
