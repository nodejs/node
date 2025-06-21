'use strict';

const { createBenchmark } = require('../common.js');

const { connect, createServer } = require('net');
const { open } = require('fs');

const bench = createBenchmark(main, {
  handlesCount: [1e4],
  requestsCount: [1e4],
  timeoutsCount: [1e4],
  immediatesCount: [1e4],
  n: [1e5],
});

function main({ handlesCount, requestsCount, timeoutsCount, immediatesCount, n }) {
  const server = createServer().listen();
  const clients = [];
  for (let i = 0; i < handlesCount; i++) {
    clients.push(connect({ port: server.address().port }));
  }

  for (let i = 0; i < requestsCount; i++) {
    open(__filename, 'r', () => {});
  }

  for (let i = 0; i < timeoutsCount; ++i) {
    setTimeout(() => {}, 1);
  }

  for (let i = 0; i < immediatesCount; ++i) {
    setImmediate(() => {});
  }

  bench.start();
  for (let i = 0; i < n; ++i) {
    process.getActiveResourcesInfo();
  }
  bench.end(n);

  for (const client of clients) {
    client.destroy();
  }
  server.close();
}
