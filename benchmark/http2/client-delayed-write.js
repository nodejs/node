'use strict';

const common = require('../common.js');
const net = require('node:net');
const { Duplex } = require('node:stream');

const WINDOW_SIZE = 32 * 1024 * 1024;

const bench = common.createBenchmark(main, {
  n: [500],
  parallel: [25],
  size: [384 * 1024],
  delay: [0, 5],
}, { flags: ['--no-warnings'] });

class DelayedWriteSocket extends Duplex {
  constructor(port, delay, host = '127.0.0.1') {
    super();
    this.delay = delay;
    this.inner = net.connect(port, host);
    this.inner.on('data', (chunk) => {
      if (!this.push(chunk)) this.inner.pause();
    });
    this.inner.on('end', () => this.push(null));
    this.inner.on('error', (err) => this.destroy(err));
    this.inner.on('close', () => this.destroy());
  }

  _read() {
    this.inner.resume();
  }

  _write(chunk, encoding, callback) {
    if (!this.inner.write(chunk, encoding)) {
      this.inner.once('drain', () => setTimeout(callback, this.delay));
      return;
    }
    setTimeout(callback, this.delay);
  }

  _final(callback) {
    this.inner.end();
    setTimeout(callback, this.delay);
  }

  _destroy(err, callback) {
    this.inner.destroy();
    callback(err);
  }
}

function once(emitter, event) {
  return new Promise((resolve, reject) => {
    emitter.once(event, resolve);
    emitter.once('error', reject);
  });
}

function fetchHttp2(client) {
  return new Promise((resolve, reject) => {
    const req = client.request({ ':path': '/' });
    let total = 0;
    req.on('data', (chunk) => {
      total += chunk.length;
    });
    req.on('end', () => resolve(total));
    req.on('error', reject);
    req.end();
  });
}

async function main({ n, parallel, size, delay }) {
  const http2 = require('node:http2');
  const payload = Buffer.alloc(size, 'x');
  const server = http2.createServer();

  server.on('stream', (stream) => {
    stream.respond({
      ':status': 200,
      'content-length': payload.length,
      'content-type': 'application/octet-stream',
    });
    stream.end(payload);
  });

  server.listen(0, '127.0.0.1');
  await once(server, 'listening');

  const port = server.address().port;
  const client = http2.connect(`http://127.0.0.1:${port}`, {
    settings: { initialWindowSize: WINDOW_SIZE },
    createConnection: delay === 0 ? undefined : () => {
      return new DelayedWriteSocket(port, delay);
    },
  });

  try {
    await once(client, 'connect');
    client.setLocalWindowSize(WINDOW_SIZE);

    // Warm up the session so connection establishment does not dominate.
    await fetchHttp2(client);

    bench.start();
    for (let completed = 0; completed < n; completed += parallel) {
      const batch = Math.min(parallel, n - completed);
      await Promise.all(
        Array.from({ length: batch }, () => fetchHttp2(client)),
      );
    }
    bench.end(n);
  } finally {
    client.close();
    server.close();
  }
}
