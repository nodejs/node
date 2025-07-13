'use strict';

const common = require('../common.js');
const dgram = require('dgram');

const bench = common.createBenchmark(main, {
  n: [5e6],
  method: ['size', 'count'],
});

function main({ n, method }) {
  const server = dgram.createSocket('udp4');
  const client = dgram.createSocket('udp4');

  server.bind(0, () => {
    client.connect(server.address().port, () => {
      for (let i = 0; i < 999; i++) client.send('Hello');

      bench.start();

      if (method === 'size') {
        for (let i = 0; i < n; ++i) server.getSendQueueSize();
      } else {
        for (let i = 0; i < n; ++i) server.getSendQueueCount();
      }

      bench.end(n);

      client.close();
      server.close();
    });
  });
}
