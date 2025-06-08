// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const { PerformanceObserver } = require('perf_hooks');

const obs = new PerformanceObserver(common.mustCallAtLeast((items) => {
  const entry = items.getEntries()[0];
  assert.strictEqual(entry.entryType, 'http2');
  assert.strictEqual(typeof entry.startTime, 'number');
  assert.strictEqual(typeof entry.duration, 'number');
  switch (entry.name) {
    case 'Http2Session':
      assert.strictEqual(typeof entry.detail.pingRTT, 'number');
      assert.strictEqual(typeof entry.detail.streamAverageDuration, 'number');
      assert.strictEqual(typeof entry.detail.streamCount, 'number');
      assert.strictEqual(typeof entry.detail.framesReceived, 'number');
      assert.strictEqual(typeof entry.detail.framesSent, 'number');
      assert.strictEqual(typeof entry.detail.bytesWritten, 'number');
      assert.strictEqual(typeof entry.detail.bytesRead, 'number');
      assert.strictEqual(typeof entry.detail.maxConcurrentStreams, 'number');
      switch (entry.detail.type) {
        case 'server':
          assert.strictEqual(entry.detail.streamCount, 1);
          assert(entry.detail.framesReceived >= 3);
          break;
        case 'client':
          assert.strictEqual(entry.detail.streamCount, 1);
          assert.strictEqual(entry.detail.framesReceived, 7);
          break;
        default:
          assert.fail('invalid Http2Session type');
      }
      break;
    case 'Http2Stream':
      assert.strictEqual(typeof entry.detail.timeToFirstByte, 'number');
      assert.strictEqual(typeof entry.detail.timeToFirstByteSent, 'number');
      assert.strictEqual(typeof entry.detail.timeToFirstHeader, 'number');
      assert.strictEqual(typeof entry.detail.bytesWritten, 'number');
      assert.strictEqual(typeof entry.detail.bytesRead, 'number');
      break;
    default:
      assert.fail('invalid entry name');
  }
}));

obs.observe({ type: 'http2' });

const body =
  '<html><head></head><body><h1>this is some data</h2></body></html>';

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  assert.strictEqual(headers[':scheme'], 'http');
  assert.ok(headers[':authority']);
  assert.strictEqual(headers[':method'], 'GET');
  assert.strictEqual(flags, 5);
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.write(body.slice(0, 20));
  stream.end(body.slice(20));
}

server.on('session', common.mustCall((session) => {
  session.ping(common.mustCall());
}));

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  client.on('connect', common.mustCall(() => {
    client.ping(common.mustCall());
  }));

  const req = client.request();

  req.on('response', common.mustCall());

  let data = '';
  req.setEncoding('utf8');
  req.on('data', (d) => data += d);
  req.on('end', common.mustCall(() => {
    assert.strictEqual(body, data);
  }));
  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));

}));
