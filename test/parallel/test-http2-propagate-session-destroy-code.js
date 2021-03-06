'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const server = http2.createServer();
const errRegEx = /Session closed with error code 7/;
const destroyCode = http2.constants.NGHTTP2_REFUSED_STREAM;

server.on('error', common.mustNotCall());

server.on('session', (session) => {
  session.on('close', common.mustCall());
  session.on('error', common.mustCall((err) => {
    assert.ok(errRegEx.test(err), `server session err: ${err}`);
    assert.strictEqual(session.closed, false);
    assert.strictEqual(session.destroyed, true);
  }));

  session.on('stream', common.mustCall((stream) => {
    stream.on('error', common.mustCall((err) => {
      assert.ok(errRegEx.test(err), `server stream err: ${err}`);
      assert.strictEqual(session.closed, false);
      assert.strictEqual(session.destroyed, true);
      assert.strictEqual(stream.rstCode, destroyCode);
    }));

    session.destroy(destroyCode);
  }));
});

server.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server.address().port}`);

  session.on('error', common.mustCall((err) => {
    assert.ok(errRegEx.test(err), `client session err: ${err}`);
    assert.strictEqual(session.closed, false);
    assert.strictEqual(session.destroyed, true);
  }));

  const stream = session.request({ [http2.constants.HTTP2_HEADER_PATH]: '/' });

  stream.on('error', common.mustCall((err) => {
    assert.ok(errRegEx.test(err), `client stream err: ${err}`);
    assert.strictEqual(stream.rstCode, destroyCode);
  }));

  stream.on('close', common.mustCall(() => {
    server.close();
  }));
}));
