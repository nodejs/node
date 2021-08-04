'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const { PerformanceObserver } = require('perf_hooks');

const server = http2.createServer();

server.on('stream', (stream, headers) => {
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  switch (headers[':path']) {
    case '/singleEnd':
      stream.end('OK');
      break;
    case '/sequentialEnd':
      stream.write('OK');
      stream.end();
      break;
    case '/delayedEnd':
      stream.write('OK', () => stream.end());
      break;
  }
});

function testRequest(path, targetFrameCount, callback) {
  const obs = new PerformanceObserver(
    common.mustCallAtLeast((list, observer) => {
      const entries = list.getEntries();
      for (let n = 0; n < entries.length; n++) {
        const entry = entries[n];
        if (entry.name !== 'Http2Session') continue;
        if (entry.detail.type !== 'client') continue;
        assert.strictEqual(entry.detail.framesReceived, targetFrameCount);
        observer.disconnect();
        callback();
      }
    }));
  obs.observe({ type: 'http2' });
  const client =
    http2.connect(`http://localhost:${server.address().port}`, () => {
      const req = client.request({ ':path': path });
      req.resume();
      req.end();
      req.on('end', () => client.close());
    });
}

// SETTINGS => SETTINGS => HEADERS => DATA
const MIN_FRAME_COUNT = 4;

server.listen(0, () => {
  testRequest('/singleEnd', MIN_FRAME_COUNT, () => {
    testRequest('/sequentialEnd', MIN_FRAME_COUNT, () => {
      testRequest('/delayedEnd', MIN_FRAME_COUNT + 1, () => {
        server.close();
      });
    });
  });
});
