'use strict';
const common = require('../common');
const http = require('http');
const cluster = require('cluster');
const assert = require('assert');

cluster.schedulingPolicy = cluster.SCHED_RR;

const server = http.createServer();

if (cluster.isMaster) {
  server.listen({port: 0}, common.mustCall(() => {
    const worker = cluster.fork({PORT: server.address().port});
    worker.on('exit', common.mustCall(() => {
      server.close();
    }));
  }));
} else {
  assert(process.env.PORT);
  process.on('uncaughtException', common.mustCall((e) => {}));
  server.listen(process.env.PORT);
  server.on('error', common.mustCall((e) => {
    cluster.worker.disconnect();
    throw e;
  }));
}
