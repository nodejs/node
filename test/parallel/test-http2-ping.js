'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const async_hooks = require('async_hooks');
const assert = require('assert');
const http2 = require('http2');
const { inspect } = require('util');

const pings = new Set();
const events = [0, 0, 0, 0];

const hook = async_hooks.createHook({
  init(id, type, trigger, resource) {
    if (type === 'HTTP2PING') {
      pings.add(id);
      events[0]++;
    }
  },
  before(id) {
    if (pings.has(id)) {
      events[1]++;
    }
  },
  after(id) {
    if (pings.has(id)) {
      events[2]++;
    }
  },
  destroy(id) {
    if (pings.has(id)) {
      events[3]++;
    }
  }
});
hook.enable();

process.on('exit', () => {
  assert.deepStrictEqual(events, [4, 4, 4, 4]);
});

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  assert(stream.session.ping(common.mustCall((err, duration, ret) => {
    assert.strictEqual(err, null);
    assert.strictEqual(typeof duration, 'number');
    assert.strictEqual(ret.length, 8);
    stream.end('ok');
  })));
  stream.respond();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`,
                               { maxOutstandingPings: 2 });
  client.on('connect', common.mustCall(() => {
    {
      const payload = Buffer.from('abcdefgh');
      assert(client.ping(payload, common.mustCall((err, duration, ret) => {
        assert.strictEqual(err, null);
        assert.strictEqual(typeof duration, 'number');
        assert.deepStrictEqual(payload, ret);
      })));
    }
    {
      const payload = Buffer.from('abcdefgi');
      assert(client.ping(payload, common.mustCall((err, duration, ret) => {
        assert.strictEqual(err, null);
        assert.strictEqual(typeof duration, 'number');
        assert.deepStrictEqual(payload, ret);
      })));
    }

    // Only max 2 pings at a time based on the maxOutstandingPings option
    assert(!client.ping(common.expectsError({
      code: 'ERR_HTTP2_PING_CANCEL',
      name: 'Error',
      message: 'HTTP2 ping cancelled'
    })));

    // Should throw if payload is not of type ArrayBufferView
    {
      [1, true, {}, []].forEach((payload) =>
        assert.throws(
          () => client.ping(payload),
          {
            name: 'TypeError',
            code: 'ERR_INVALID_ARG_TYPE',
            message: 'The "payload" argument must be an instance of Buffer, ' +
                     'TypedArray, or DataView.' +
                     common.invalidArgTypeHelper(payload)
          }
        )
      );
    }

    // Should throw if payload length is not 8
    {
      const shortPayload = Buffer.from('abcdefg');
      const longPayload = Buffer.from('abcdefghi');
      [shortPayload, longPayload].forEach((payloadWithInvalidLength) =>
        assert.throws(
          () => client.ping(payloadWithInvalidLength),
          {
            name: 'RangeError',
            code: 'ERR_HTTP2_PING_LENGTH',
            message: 'HTTP2 ping payload must be 8 bytes'
          }
        )
      );
    }

    // Should throw error is callback is not of type function
    {
      const payload = Buffer.from('abcdefgh');
      [1, true, {}, []].forEach((invalidCallback) =>
        assert.throws(
          () => client.ping(payload, invalidCallback),
          {
            name: 'TypeError',
            code: 'ERR_INVALID_CALLBACK',
            message: 'Callback must be a function. ' +
                     `Received ${inspect(invalidCallback)}`
          }
        )
      );
    }

    const req = client.request();
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}));
