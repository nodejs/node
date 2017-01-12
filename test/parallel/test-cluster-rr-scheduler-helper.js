'use strict';
const common = require('../common');
const cluster = require('cluster');
const http = require('http');
const assert = require('assert');
// Want to make sure it's RR on all platforms
const PAUSE = 'cluster.pause';
const PAUSED = 'cluster.paused';
const UNPAUSE = 'cluster.unpause';
const UNPAUSED = 'cluster.unpaused';
const READY = 'cluster.ready';
const capacity = 5;
const maxNumInactiveWorkers = 2;
var inactiveWorkerCounter = 0;
var testCounter = 0;
var minTestRunCount = 20;
var scheduler = [];
if (cluster.isMaster) {
  cluster.schedulingPolicy = cluster.SCHED_RR;
  var ready = 0;
  const schedulerCb = function(handles) {
    cluster.on(PAUSE, (id) => {
      var clusterKey = Object.keys(handles)[0];
      var worker = handles[clusterKey].all[id] || null;
      var index = handles[clusterKey].free.indexOf(worker);
      if (index !== -1) {
        handles[clusterKey].free.splice(index, 1);
        getSeveralTimes((result) => {
          assert(!result.includes(id));//worker should be inactive
          for (var i = 0 ; i < (scheduler.length - maxNumInactiveWorkers) ; i++)
            assert(result.includes(scheduler[i].id)); //current active workers
          worker.send(PAUSED);//worker stops taking new incoming requests
        });
      }
    });
    cluster.on(UNPAUSED, (id) => {
      var clusterKey = Object.keys(handles)[0];
      var worker = handles[clusterKey].all[id] || null;
      var index = handles[clusterKey].free.indexOf(worker);
      if (index === -1) {
        handles[clusterKey].free.push(worker);
        getSeveralTimes((result) => {
          assert(result.includes(id));//worker should be active now
          inactiveWorkerCounter--;
        });
      }
    });
  };
  cluster.setupMaster({scheduler: schedulerCb});
  for (var i = 0 ; i < capacity ; i++) {
    const worker = cluster.fork();
    scheduler.push(worker);
  }
  cluster.on('message', (w, msg) => {
    if (msg === READY) {
      ready++;
      if (ready === capacity) runTest(scheduler);
    } else if (msg === UNPAUSE) {
      cluster.emit(UNPAUSED, w.id);
    }
  });
  function runTest(scheduler) {
    setInterval(() => {
      while (inactiveWorkerCounter < maxNumInactiveWorkers) {
        var worker = scheduler.shift();
        scheduler.push(worker);
        cluster.emit(PAUSE, worker.id);
        inactiveWorkerCounter++;
      }
    }, 100);
    setTimeout(() => {
      if (testCounter > minTestRunCount) assert(true);
      else assert(false);
      process.exit(0);
    }, 5000);//run it for 5 seconds
  }
  function getSeveralTimes(cb) {
    const results = [];
    testCounter++;
    function getOnce() {
      http.get('http://localhost:' + common.PORT, (res) => {
        results.push(parseInt(res.headers.id));
        if (results.length === (capacity * 20)) cb(results);
        else getOnce();
      });
    }
    getOnce();
  }
} else {
  http.createServer((req, res) => {
    res.setHeader('id', cluster.worker.id);
    res.end();
  }).listen(common.PORT, () => {
    process.send(READY);
  }).on('error', (e) => {});
}
process.on('message', (msg) => {
  if (msg === PAUSED) {
    //do something such as run GC, when it's done notify master to unpause it
    process.send(UNPAUSE);
  }
});
