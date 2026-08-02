'use strict';

const common = require('../common.js');
const protocols = process.versions.openssl ? ['http', 'https'] : ['http'];

const configs = {
  sameTurn: [{
    type: ['bytes', 'buffer', 'uint8array'],
    len: [64, 1024],
    chunks: [1, 2, 4, 16],
    mode: ['auto', 'explicit'],
    transfer: ['chunked', 'length'],
    protocol: protocols,
    producer: ['sync'],
    callback: [0],
    c: [50],
    duration: 5,
  }],
  streaming: [{
    type: ['bytes', 'buffer', 'uint8array'],
    len: [64, 1024],
    chunks: [4],
    mode: ['auto'],
    transfer: ['chunked'],
    protocol: protocols,
    producer: ['nextTick', 'microtask', 'immediate'],
    callback: [0],
    c: [50],
    duration: 5,
  }],
  callbacks: [{
    type: ['bytes', 'buffer', 'uint8array'],
    len: [64],
    chunks: [4, 16],
    mode: ['auto', 'explicit'],
    transfer: ['chunked'],
    protocol: protocols,
    producer: ['sync'],
    callback: [1],
    c: [50],
    duration: 5,
  }],
  fixedBody: [{
    type: ['bytes', 'buffer', 'uint8array'],
    total: [64 * 1024],
    chunks: [1, 4, 16, 128],
    mode: ['auto', 'explicit'],
    transfer: ['chunked'],
    protocol: protocols,
    producer: ['sync'],
    callback: [0],
    c: [50],
    duration: 5,
  }],
  largeChunks: [{
    type: ['bytes', 'buffer', 'uint8array'],
    len: [4 * 1024, 8 * 1024, 16 * 1024, 64 * 1024],
    chunks: [1, 4],
    mode: ['auto'],
    transfer: ['chunked'],
    protocol: protocols,
    producer: ['sync'],
    callback: [0],
    c: [50],
    duration: 5,
  }],
  concurrency: [{
    type: ['bytes'],
    len: [64],
    chunks: [4],
    mode: ['auto'],
    transfer: ['chunked'],
    protocol: protocols,
    producer: ['sync'],
    callback: [0],
    c: [1, 50, 500],
    duration: 5,
  }],
};

const bench = common.createBenchmark(main, configs, { byGroups: true });

function main({
  type,
  len,
  chunks,
  mode,
  transfer,
  protocol,
  producer,
  callback,
  c,
  duration,
  total,
}) {
  const transport = require(protocol);
  len ??= total / chunks;
  const chunk = type === 'bytes' ? 'a'.repeat(len) :
    type === 'buffer' ? Buffer.alloc(len, 'a') :
      new Uint8Array(len).fill(0x61);
  const writeCallback = callback ? (err) => {
    if (err) throw err;
  } : undefined;

  const schedule = producer === 'nextTick' ? process.nextTick :
    producer === 'microtask' ? queueMicrotask : setImmediate;

  const onRequest = (req, res) => {
    if (transfer === 'length') {
      res.setHeader('Content-Length', len * chunks);
    }
    if (mode === 'explicit') {
      res.cork();
    }

    if (producer === 'sync') {
      for (let i = 0; i < chunks; i++) {
        res.write(chunk, writeCallback);
      }
      res.end();
      return;
    }

    let written = 0;
    function writeNext() {
      if (written++ === chunks) {
        res.end();
        return;
      }
      res.write(chunk, writeCallback);
      schedule(writeNext);
    }
    writeNext();
  };

  let server;
  if (protocol === 'https') {
    const fixtures = require('../../test/common/fixtures');
    server = transport.createServer({
      key: fixtures.readKey('rsa_private.pem'),
      cert: fixtures.readKey('rsa_cert.crt'),
    }, onRequest);
  } else {
    server = transport.createServer(onRequest);
  }

  server.listen(0, () => {
    bench.http({
      connections: c,
      duration,
      port: server.address().port,
      scheme: protocol,
    }, () => {
      server.close();
    });
  });
}
