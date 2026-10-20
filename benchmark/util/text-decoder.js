'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  encoding: ['utf-8', 'windows-1252', 'iso-8859-3'],
  ignoreBOM: [0, 1],
  fatal: [0, 1],
  type: ['SharedArrayBuffer', 'ArrayBuffer', 'Buffer'],
  content: ['ascii', 'one-byte-string', 'two-byte-string'],
  len: [256, 1024 * 16, 1024 * 128],
  n: [1e3],
});

function buildContent(content, len) {
  let base;
  switch (content) {
    case 'ascii': base = 'a'; break;
    case 'one-byte-string': base = '\xff'; break;
    case 'two-byte-string': base = 'ğ'; break;
  }
  const unitBytes = Buffer.byteLength(base, 'utf8');
  const copies = Math.max(1, Math.floor(len / unitBytes));
  return Buffer.from(base.repeat(copies));
}

function main({ encoding, len, n, ignoreBOM, type, fatal, content }) {
  const decoder = new TextDecoder(encoding, { ignoreBOM, fatal });
  const seed = buildContent(content, len);
  let buf;

  switch (type) {
    case 'SharedArrayBuffer': {
      buf = new SharedArrayBuffer(seed.length);
      new Uint8Array(buf).set(seed);
      break;
    }
    case 'ArrayBuffer': {
      buf = new ArrayBuffer(seed.length);
      new Uint8Array(buf).set(seed);
      break;
    }
    case 'Buffer': {
      buf = seed;
      break;
    }
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      decoder.decode(buf);
    } catch {
      // eslint-disable no-empty
    }
  }
  bench.end(n);
}
