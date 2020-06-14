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
      // Backport v10.x: Manually pack END_STREAM flag
      stream._final(() => {});
      break;
    case '/sequentialEnd':
      stream.write('OK');
      stream.end();
      // Backport v10.x: Manually pack END_STREAM flag
      stream._final(() => {});
      break;
    case '/delayedEnd':
      stream.write('OK', () => stream.end());
      break;
  }
});

function testRequest(path, targetFrameCount, callback) {
  const obs = new PerformanceObserver((list, observer) => {
    const entry = list.getEntries()[0];
    if (entry.name !== 'Http2Session') return;
    if (entry.type !== 'client') return;
    assert.strictEqual(entry.framesReceived, targetFrameCount);
    observer.disconnect();
    callback();
  });
  obs.observe({ entryTypes: ['http2'] });
  const client = http2.connect(`http://localhost:${server.address().port}`, () => {
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
