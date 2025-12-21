'use strict';

const common = require('../common.js');
const dgram = require('dgram');
const { Buffer } = require('buffer');

const bench = common.createBenchmark(main, {
  type: ['string', 'buffer', 'mixed', 'typedarray'],
  chunks: [1, 4, 8, 16],
  len: [64, 512, 1024],
  n: [1000],
});

function main({ type, chunks, len, n }) {
  const socket = dgram.createSocket('udp4');

  let testData;
  switch (type) {
    case 'string':
      testData = Array(chunks).fill('a'.repeat(len));
      break;
    case 'buffer':
      testData = Array(chunks).fill(Buffer.alloc(len, 'a'));
      break;
    case 'mixed':
      testData = [];
      for (let i = 0; i < chunks; i++) {
        if (i % 2 === 0) {
          testData.push(Buffer.alloc(len, 'a'));
        } else {
          testData.push('a'.repeat(len));
        }
      }
      break;
    case 'typedarray':
      testData = Array(chunks).fill(new Uint8Array(len).fill(97));
      break;
  }

  bench.start();

  for (let i = 0; i < n; i++) {
    socket.send(testData, 12345, 'localhost', (err) => {
      if (err && err.code !== 'ENOTCONN' && err.code !== 'ECONNREFUSED') {
        throw err;
      }
    });
  }

  bench.end(n);
  socket.close();
}
