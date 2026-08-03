'use strict';

const common = require('../common.js');
const protocols = process.versions.openssl ? ['http', 'https'] : ['http'];

const scenarios = {
  'end-64': {
    len: 64,
    chunks: 1,
    endChunk: true,
  },
  'end-1024': {
    len: 1024,
    chunks: 1,
    endChunk: true,
  },
  'end-1025': {
    len: 1025,
    chunks: 1,
    endChunk: true,
  },
  'auto-4': {
    len: 64,
    chunks: 4,
  },
  'auto-16': {
    len: 64,
    chunks: 16,
  },
  'explicit-16': {
    len: 64,
    chunks: 16,
    explicit: true,
  },
  'content-length-16': {
    len: 64,
    chunks: 16,
    contentLength: true,
  },
  'next-tick-4': {
    len: 64,
    chunks: 4,
    schedule: process.nextTick,
  },
  'callbacks-16': {
    len: 64,
    chunks: 16,
    callbacks: true,
  },
  'fixed-body-128': {
    len: 512,
    chunks: 128,
  },
  'large-16k': {
    len: 16 * 1024,
    chunks: 4,
  },
};

const bench = common.createBenchmark(main, {
  type: ['string', 'buffer', 'uint8array'],
  scenario: Object.keys(scenarios),
  protocol: protocols,
  c: [50],
  duration: [5],
});

function main({ type, scenario, protocol, c, duration }) {
  const {
    callbacks,
    chunks,
    contentLength,
    endChunk,
    explicit,
    len,
    schedule,
  } = scenarios[scenario];
  const transport = require(protocol);
  const chunk = type === 'string' ? 'a'.repeat(len) :
    type === 'buffer' ? Buffer.alloc(len, 'a') :
      new Uint8Array(len).fill(0x61);
  const writeCallback = callbacks ? (err) => {
    if (err) throw err;
  } : undefined;

  const onRequest = (req, res) => {
    if (contentLength) {
      res.setHeader('Content-Length', len * chunks);
    }
    if (explicit) {
      res.cork();
    }
    if (endChunk) {
      res.end(chunk, writeCallback);
      return;
    }

    if (schedule === undefined) {
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
