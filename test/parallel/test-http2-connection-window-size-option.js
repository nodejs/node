'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

const DEFAULT_WINDOW = 33554432;
const SPEC_DEFAULT_WINDOW = 65535;

// Set client & server's windows (or default if undefined) and validate that
// the both peers see the remote value as expected.
function check(serverWindow, clientWindow) {
  const server = http2.createServer({ connectionWindowSize: serverWindow });
  serverWindow ??= DEFAULT_WINDOW;
  clientWindow ??= DEFAULT_WINDOW;

  server.on('session', common.mustCall((session) => {
    assert.strictEqual(session.state.effectiveLocalWindowSize, serverWindow);
  }));

  server.on('stream', common.mustCall((stream) => {
    assert.strictEqual(stream.session.state.remoteWindowSize, clientWindow);
    stream.respond({ ':status': 200 }, { endStream: true });
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`,
                                 { connectionWindowSize: clientWindow });

    client.on('connect', common.mustCall(() => {
      assert.strictEqual(client.state.effectiveLocalWindowSize, clientWindow);
    }));

    const req = client.request();
    req.resume();
    req.on('close', common.mustCall(() => {
      assert.strictEqual(client.state.remoteWindowSize, serverWindow);
      client.close();
      server.close();
    }));
  }));
}

check(undefined, undefined);

// The option shrinks the window below the default, which setLocalWindowSize()
// cannot do once the connection has been established.
check(SPEC_DEFAULT_WINDOW, SPEC_DEFAULT_WINDOW);

// Each end of the connection is configured independently.
check(2 ** 20, 2 ** 21);

// A window below the peer's fixed initial 65535 is the only case that shrinks
// the window nghttp2 has already accounted for, leaving a reduction to pay off
// before any WINDOW_UPDATE can be sent again. Larger windows, including the
// default, only ever grow it. Send more than the window to prove the
// connection still resumes rather than stalling once the reduction is paid.
{
  const windowSize = 16384;
  const payload = Buffer.alloc(2 ** 18, 'x');
  const server = http2.createServer({ connectionWindowSize: windowSize });

  server.on('stream', common.mustCall((stream) => {
    let received = 0;
    stream.on('data', (chunk) => { received += chunk.length; });
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(received, payload.length);
      stream.respond({ ':status': 200 }, { endStream: true });
    }));
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`,
                                 { connectionWindowSize: windowSize });
    const req = client.request({ ':method': 'POST', ':path': '/' });
    req.resume();
    req.end(payload);
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

for (const connectionWindowSize of [-1, 0, 2 ** 31, 1.5]) {
  assert.throws(() => http2.createServer({ connectionWindowSize }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => http2.createSecureServer({ connectionWindowSize }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => http2.connect('http://localhost:80', { connectionWindowSize }),
                { code: 'ERR_OUT_OF_RANGE' });
}

// Windows below the peer's fixed initial 65535 are permitted: they cannot
// shrink that first burst, but they do throttle everything after it.
for (const connectionWindowSize of [1, 1024, 65534]) {
  http2.createServer({ connectionWindowSize }).close();
}

for (const connectionWindowSize of ['1024', null, {}]) {
  assert.throws(() => http2.createServer({ connectionWindowSize }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => http2.connect('http://localhost:80', { connectionWindowSize }),
                { code: 'ERR_INVALID_ARG_TYPE' });
}
