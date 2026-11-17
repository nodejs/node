'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { once } = require('events');
const http2 = require('http2');
const { setImmediate } = require('timers/promises');

const { NGHTTP2_CANCEL, NGHTTP2_NO_ERROR } = http2.constants;

// Canceling a queued upload must release its remaining session memory credit,
// without disrupting another stream or preventing new requests.
async function test(initialWindowSize, destroyInWriteCallback = false) {
  const server = http2.createServer({ settings: { initialWindowSize } });
  const sessions = new Set();
  let survivingStream;
  let received = 0;
  server.on('session', (session) => sessions.add(session));
  server.on('stream', common.mustCall((stream, headers) => {
    if (headers[':method'] === 'POST') {
      if (destroyInWriteCallback) {
        stream.on('error', common.mustCall((error) => {
          assert.strictEqual(error.code, 'ERR_HTTP2_STREAM_ABORTED');
        }));
      } else if (initialWindowSize === 0) {
        stream.close(NGHTTP2_CANCEL);
      } else {
        stream.once('data', common.mustCall((chunk) => {
          received += chunk.length;
          stream.close(NGHTTP2_CANCEL);
        }));
      }
      return;
    }
    stream.respond({ ':status': 200 });
    if (headers[':path'] === '/survivor') {
      survivingStream = stream;
    } else {
      stream.end();
      survivingStream.end('still alive');
    }
  }, 3));

  let client;
  try {
    server.listen(0, '127.0.0.1');
    await once(server, 'listening');
    client = http2.connect(`http://127.0.0.1:${server.address().port}`, {
      maxSessionMemory: 1,
    });
    const [settings] = await once(client, 'remoteSettings');
    assert.strictEqual(settings.initialWindowSize, initialWindowSize);

    const survivor = client.request({ ':path': '/survivor' });
    let body = '';
    survivor.setEncoding('utf8');
    survivor.on('data', (chunk) => { body += chunk; });
    const survived = once(survivor, 'end');
    const [headers] = await once(survivor, 'response');
    assert.strictEqual(headers[':status'], 200);

    const upload = client.request({ ':method': 'POST' });
    if (destroyInWriteCallback) {
      upload.on('error', common.mustNotCall());
      // Completing this empty native write dispatches the buffered body before
      // its user callback destroys the stream, while DATA is being gathered.
      upload.write('', common.mustCall(() => upload.destroy()));
    } else {
      upload.on('error', common.mustCall((error) => {
        assert.strictEqual(error.code, 'ERR_HTTP2_STREAM_ABORTED');
      }));
    }
    const closed = new Promise((resolve) => upload.once('close', resolve));
    upload.write(Buffer.alloc(4 * 1024 * 1024), common.mustCall());
    await closed;
    assert.strictEqual(upload.rstCode,
                       destroyInWriteCallback ? NGHTTP2_NO_ERROR : NGHTTP2_CANCEL);
    if (initialWindowSize === 0) {
      assert.strictEqual(received, 0);
    } else if (!destroyInWriteCallback) {
      assert(received > 0);
      assert(received < 4 * 1024 * 1024);
    }

    // Native stream destruction frees the queued write in an immediate.
    await setImmediate();
    const request = client.request();
    const completed = Promise.all([
      once(request, 'response'),
      once(request, 'end'),
    ]);
    request.resume();
    const [[responseHeaders]] = await completed;
    assert.strictEqual(responseHeaders[':status'], 200);
    await survived;
    assert.strictEqual(body, 'still alive');
  } finally {
    client?.destroy();
    for (const session of sessions) session.destroy();
    server.close();
  }
}

(async () => {
  await test(0);
  await test(64 * 1024);
  await test(64 * 1024, true);
})().then(common.mustCall());
