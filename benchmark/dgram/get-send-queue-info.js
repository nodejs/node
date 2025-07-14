'use strict';

const common = require('../common.js');
const dgram = require('dgram');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [5e6],
  method: ['size', 'count'],
}, {
  flags: ['--test-udp-no-try-send'],
});

function main({ n, method }) {
  const server = dgram.createSocket('udp4');
  const client = dgram.createSocket('udp4');

  server.bind(0, () => {
    client.connect(server.address().port, () => {
      // warm-up: keep packets unsent using --test-udp-no-try-send flag
      for (let i = 0; i < 999; i++) client.send('Hello');

      let size = 0;
      let count = 0;

      bench.start();

      if (method === 'size') {
        for (let i = 0; i < n; ++i) size += client.getSendQueueSize();
      } else {
        for (let i = 0; i < n; ++i) count += client.getSendQueueCount();
      }

      bench.end(n);

      client.close();
      server.close();

      assert.ok(size >= 0);
      assert.ok(count >= 0);
    });
  });
}
