'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const net = require('net');

tmpdir.refresh();

const { PerformanceObserver } = require('perf_hooks');

const entries = [];

const obs = new PerformanceObserver(common.mustCallAtLeast((items) => {
  entries.push(...items.getEntries());
}));

obs.observe({ type: 'net' });

{
  const server = net.createServer(common.mustCall((socket) => {
    socket.destroy();
  }));

  server.listen(0, common.mustCall(async () => {
    await new Promise((resolve, reject) => {
      const socket = net.connect(server.address().port);
      socket.on('end', resolve);
      socket.on('error', reject);
    });
    server.close();
  }));
}

{
  const server = net.createServer(common.mustCall((socket) => {
    socket.destroy();
  }));

  server.listen(common.PIPE, common.mustCall(async () => {
    await new Promise((resolve, reject) => {
      const socket = net.connect(common.PIPE);
      socket.on('end', resolve);
      socket.on('error', reject);
    });
    server.close();
  }));
}

process.on('exit', () => {
  assert.strictEqual(entries.length, 1);
  for (const entry of entries) {
    assert.strictEqual(entry.name, 'connect');
    assert.strictEqual(entry.entryType, 'net');
    assert.strictEqual(typeof entry.startTime, 'number');
    assert.strictEqual(typeof entry.duration, 'number');
    assert.strictEqual(!!entry.detail.host, true);
    assert.strictEqual(!!entry.detail.port, true);
  }
});
