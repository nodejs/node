// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// Tests that, when receiving small chunks, we do not keep the full length
// of the original allocation for the libuv read call in memory.

let client;
let baseRSS;
const receivedChunks = [];
const N = 250000;

const server = net.createServer(common.mustCall((socket) => {
  baseRSS = process.memoryUsage().rss;

  socket.setNoDelay(true);
  socket.on('data', (chunk) => {
    receivedChunks.push(chunk);
    if (receivedChunks.length < N) {
      client.write('a');
    } else {
      client.end();
      server.close();
    }
  });
})).listen(0, common.mustCall(() => {
  client = net.connect(server.address().port);
  client.setNoDelay(true);
  client.write('hello!');
}));

process.on('exit', () => {
  global.gc();
  const bytesPerChunk =
    (process.memoryUsage().rss - baseRSS) / receivedChunks.length;
  // We should always have less than one page (usually ~ 4 kB) per chunk.
  assert(bytesPerChunk < 512, `measured ${bytesPerChunk} bytes per chunk`);
});
