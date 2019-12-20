'use strict';

const cluster = require('cluster');
if (cluster.isMaster) {
  const common = require('../common.js');
  const bench = common.createBenchmark(main, {
    workers: [1],
    payload: ['string', 'object'],
    sendsPerBroadcast: [1, 10],
    serialization: ['json', 'advanced'],
    n: [1e5]
  });

  function main({
    n,
    workers,
    sendsPerBroadcast,
    payload,
    serialization
  }) {
    const expectedPerBroadcast = sendsPerBroadcast * workers;
    let readies = 0;
    let broadcasts = 0;
    let msgCount = 0;
    let data;

    cluster.settings.serialization = serialization;

    switch (payload) {
      case 'string':
        data = 'hello world!';
        break;
      case 'object':
        data = { action: 'pewpewpew', powerLevel: 9001 };
        break;
      default:
        throw new Error('Unsupported payload type');
    }

    for (let i = 0; i < workers; ++i)
      cluster.fork().on('online', onOnline).on('message', onMessage);

    function onOnline() {
      if (++readies === workers) {
        bench.start();
        broadcast();
      }
    }

    function broadcast() {
      if (broadcasts++ === n) {
        bench.end(n);
        for (const id in cluster.workers)
          cluster.workers[id].disconnect();
        return;
      }
      for (const id in cluster.workers) {
        const worker = cluster.workers[id];
        for (let i = 0; i < sendsPerBroadcast; ++i)
          worker.send(data);
      }
    }

    function onMessage() {
      if (++msgCount === expectedPerBroadcast) {
        msgCount = 0;
        broadcast();
      }
    }
  }
} else {
  process.on('message', (msg) => {
    process.send(msg);
  });
}
