// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

server.on('stream', common.mustCall(onStream));

function assertSettings(settings) {
  assert.strictEqual(typeof settings, 'object');
  assert.strictEqual(typeof settings.headerTableSize, 'number');
  assert.strictEqual(typeof settings.enablePush, 'boolean');
  assert.strictEqual(typeof settings.initialWindowSize, 'number');
  assert.strictEqual(typeof settings.maxFrameSize, 'number');
  assert.strictEqual(typeof settings.maxConcurrentStreams, 'number');
  assert.strictEqual(typeof settings.maxHeaderListSize, 'number');
}

function onStream(stream, headers, flags) {

  assertSettings(stream.session.localSettings);
  assertSettings(stream.session.remoteSettings);

  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('hello world');
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`, {
    settings: {
      enablePush: false,
      initialWindowSize: 123456
    }
  });

  client.on('localSettings', common.mustCall((settings) => {
    assert(settings);
    assert.strictEqual(settings.enablePush, false);
    assert.strictEqual(settings.initialWindowSize, 123456);
    assert.strictEqual(settings.maxFrameSize, 16384);
  }, 2));
  client.on('remoteSettings', common.mustCall((settings) => {
    assert(settings);
  }));

  const headers = { ':path': '/' };

  const req = client.request(headers);

  req.on('connect', common.mustCall(() => {
    // pendingSettingsAck will be true if a SETTINGS frame
    // has been sent but we are still waiting for an acknowledgement
    assert(client.pendingSettingsAck);
  }));

  // State will only be valid after connect event is emitted
  req.on('ready', common.mustCall(() => {
    assert.doesNotThrow(() => {
      client.settings({
        maxHeaderListSize: 1
      });
    });

    // Verify valid error ranges
    [
      ['headerTableSize', -1],
      ['headerTableSize', 2 ** 32],
      ['initialWindowSize', -1],
      ['initialWindowSize', 2 ** 32],
      ['maxFrameSize', 16383],
      ['maxFrameSize', 2 ** 24],
      ['maxHeaderListSize', -1],
      ['maxHeaderListSize', 2 ** 32]
    ].forEach((i) => {
      const settings = {};
      settings[i[0]] = i[1];
      assert.throws(() => client.settings(settings),
                    common.expectsError({
                      code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
                      type: RangeError
                    }));
    });
    [1, {}, 'test', [], null, Infinity, NaN].forEach((i) => {
      assert.throws(() => client.settings({ enablePush: i }),
                    common.expectsError({
                      code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
                      type: TypeError
                    }));
    });

  }));

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();

}));
