'use strict';
var common = require('../common');
var cluster = require('cluster');
var domain = require('domain');

// RR is the default for v0.11.9+ so the following line is redundant:
// cluster.schedulingPolicy = cluster.SCHED_RR;

if (cluster.isWorker) {
  var d = domain.create();
  d.run(function() { });

  var http = require('http');
  http.Server(function() { }).listen(common.PORT, '127.0.0.1');

} else if (cluster.isMaster) {
  var worker;

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
