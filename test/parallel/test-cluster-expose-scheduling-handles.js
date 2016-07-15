'use strict';
const common = require('../common');
const cluster = require('cluster');
const http = require('http');
const assert = require('assert');
// Want to make sure it's RR on all platforms
cluster.schedulingPolicy = cluster.SCHED_RR;
if (cluster.isMaster) {
  cluster.setupMaster({shouldExposeSchedulingHandles: true});
  let ready = 0;
  const worker1 = cluster.fork();
  const worker2 = cluster.fork();
  const onMessage = (msg) => {
    if (msg === 'ready') {
      ready++;
      if (ready === 2) runTest(worker1, worker2);
    }
  };
  worker1.on('message', onMessage);
  worker2.on('message', onMessage);
} else {
  http.createServer((req, res) => {
    res.setHeader('id', cluster.worker.id);
    res.end();
  }).listen(common.PORT, () => {
    process.send('ready');
  });
}
function runTest(worker1, worker2) {
  assert(cluster.handles !== undefined);
  worker1.kill();
  worker2.kill();
}
