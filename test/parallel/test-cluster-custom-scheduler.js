'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const http = require('http');
const net = require('net');

if (cluster.isMaster) {
  const numWorkers = 2;
  const pattern = [2, 1, 2, 2, 1, 2, 1, 1, 2];
  let index = 0;
  let readyCount = 0;

  // The scheduler moves through pattern. Each request is scheduled to the
  // worker id specified in the current pattern index.
  function scheduler(workers, socket) {
    const id = pattern[index];
    const worker = workers.filter((w) => {
      return w.id === id;
    }).pop();

    if (id === 2) {
      assert.strictEqual(scheduler.exposeSocket, true);
      assert(socket instanceof net.Socket);
    } else {
      assert.strictEqual(scheduler.exposeSocket, false);
      assert.strictEqual(socket, undefined);
    }

    if (worker !== undefined)
      index++;

    return worker;
  }

  // Create a getter for exposeSocket. If the current item in the pattern is 2,
  // then expose the socket. Otherwise, hide it.
  Object.defineProperty(scheduler, 'exposeSocket', {
    get() { return pattern[index] === 2; }
  });

  function onMessage(msg) {
    // Once both workers send a 'ready' signal, indicating that their servers
    // are listening, begin making HTTP requests.
    assert.strictEqual(msg.cmd, 'ready');
    readyCount++;

    if (readyCount === numWorkers)
      makeRequest(0, msg.port);
  }

  function makeRequest(reqCount, port) {
    // Make one request for each element in pattern and then shut down the
    // workers.
    if (reqCount >= pattern.length) {
      for (const id in cluster.workers)
        cluster.workers[id].disconnect();

      return;
    }

    http.get({ port }, (res) => {
      res.on('data', (data) => {
        assert.strictEqual(+data.toString(), pattern[reqCount]);
        reqCount++;
        makeRequest(reqCount, port);
      });
    });
  }

  cluster.schedulingPolicy = cluster.SCHED_CUSTOM;
  cluster.setupMaster({ scheduler });

  for (let i = 0; i < numWorkers; i++)
    cluster.fork().on('message', common.mustCall(onMessage));

} else {
  const server = http.createServer((req, res) => {
    res.end(cluster.worker.id + '');
  }).listen(0, () => {
    process.send({ cmd: 'ready', port: server.address().port });
  });
}
