'use strict';
const common = require('../common');
const cluster = require('cluster');
const http = require('http');
const assert = require('assert');
// Want to make sure it's RR on all platforms
cluster.schedulingPolicy = cluster.SCHED_RR;
if (cluster.isMaster) {
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
function getSeveralTimes(cb) {
  const results = [];
  function getOnce() {
    http.get('http://localhost:' + common.PORT, (res) => {
      results.push(parseInt(res.headers.id));
      if (results.length === 6) cb(results);
      else getOnce();
    });
  }
  getOnce();
}
function runTest(worker1, worker2) {
  getSeveralTimes((result) => {
    assert(result.includes(1));
    assert(result.includes(2));
    cluster.pause(worker1);
    getSeveralTimes((result) => {
      assert(!result.includes(1));
      assert(result.includes(2));
      cluster.unpause(worker1);
      getSeveralTimes((result) => {
        assert(result.includes(1));
        assert(result.includes(2));
        worker1.kill();
        worker2.kill();
      });
    });
  });
}
