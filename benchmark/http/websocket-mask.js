'use strict';

const common = require('../common.js');
const { websocketMask, websocketUnmask } = require('node:http');

const bench = common.createBenchmark(main, {
  type: ['mask', 'unmask'],
  // 'http' uses http.websocketMask() / http.websocketUnmask(). 'js' is the
  // byte-by-byte loop that userland WebSocket implementations fall back to
  // without a native addon, for comparison.
  impl: ['http', 'js'],
  len: [4, 16, 125, 1024, 16384, 65536],
  n: [1e6],
});

function jsMask(source, key, output, offset, length) {
  for (let i = 0; i < length; i++) {
    output[offset + i] = source[i] ^ key[i & 3];
  }
}

function jsUnmask(buffer, key) {
  for (let i = 0; i < buffer.length; i++) {
    buffer[i] ^= key[i & 3];
  }
}

function main({ n, type, impl, len }) {
  const key = Buffer.from([0x12, 0x34, 0x56, 0x78]);
  const source = Buffer.alloc(len, 'abcdefg');
  // Leave room for a WebSocket frame header before the payload.
  const output = Buffer.alloc(len + 14);

  switch (type) {
    case 'mask': {
      const fn = impl === 'http' ? websocketMask : jsMask;
      bench.start();
      for (let i = 0; i < n; i++) {
        fn(source, key, output, 14, len);
      }
      bench.end(n);
      break;
    }
    case 'unmask': {
      const fn = impl === 'http' ? websocketUnmask : jsUnmask;
      bench.start();
      for (let i = 0; i < n; i++) {
        fn(source, key);
      }
      bench.end(n);
      break;
    }
    default:
      throw new Error(`Unexpected type: ${type}`);
  }
}
