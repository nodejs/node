'use strict';
const common = require('../common');
const cluster = require('cluster');
const domain = require('domain');

// RR is the default for v0.11.9+ so the following line is redundant:
// cluster.schedulingPolicy = cluster.SCHED_RR;

if (cluster.isWorker) {
  const d = domain.create();
  d.run(function() { });

  const http = require('http');
  http.Server(function() { }).listen(common.PORT, '127.0.0.1');

} else if (cluster.isMaster) {
  let worker;

  //Kill worker when listening
  cluster.on('listening', function() {
    worker.kill();
  });

  //Kill process when worker is killed
  cluster.on('exit', function() {
    process.exit(0);
  });

  //Create worker
  worker = cluster.fork();
}
