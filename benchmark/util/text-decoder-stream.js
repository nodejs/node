'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  encoding: ['utf-8', 'utf-16le'],
  ignoreBOM: [0, 1],
  fatal: [0, 1],
  unicode: [0, 1],
  len: [256, 1024 * 16, 1024 * 128],
  chunks: [10],
  n: [1e3],
  type: ['SharedArrayBuffer', 'ArrayBuffer', 'Buffer'],
});

const UNICODE_ALPHA = 'Blåbærsyltetøy';
const ASCII_ALPHA = 'Blueberry jam';

function main({ encoding, len, unicode, chunks, n, ignoreBOM, type, fatal }) {
  const decoder = new TextDecoder(encoding, { ignoreBOM, fatal });
  let buf;

  const fill = Buffer.from(unicode ? UNICODE_ALPHA : ASCII_ALPHA, encoding);

  switch (type) {
    case 'SharedArrayBuffer': {
      buf = new SharedArrayBuffer(len);
      Buffer.from(buf).fill(fill);
      break;
    }
    case 'ArrayBuffer': {
      buf = new ArrayBuffer(len);
      Buffer.from(buf).fill(fill);
      break;
    }
    case 'Buffer': {
      buf = Buffer.alloc(len, fill);
      break;
    }
  }

  const chunk = Math.ceil(len / chunks);
  const max = len - chunk;
  bench.start();
  for (let i = 0; i < n; i++) {
    let pos = 0;
    while (pos < max) {
      decoder.decode(buf.slice(pos, pos + chunk), { stream: true });
      pos += chunk;
    }

    decoder.decode(buf.slice(pos));
  }
  bench.end(n);
}
