'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const async_hooks = require('async_hooks');
const assert = require('assert');
const http2 = require('http2');

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
      type: Error,
      message: 'HTTP2 ping cancelled'
    })));
    const req = client.request();
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}));
