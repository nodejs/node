'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');

// The callback passed to writeContinue(), writeEarlyHints() and
// writeInformation() is called asynchronously, in order, once the
// informational headers have been written to the socket.
{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    const { socket } = res.stream.session;
    const bytesWritten = socket.bytesWritten;
    const order = [];
    let sync = true;

    assert.strictEqual(res.writeContinue(common.mustCall((err) => {
      assert.strictEqual(err, null);
      assert.strictEqual(sync, false);
      assert.ok(socket.bytesWritten > bytesWritten);
      order.push(100);
    })), true);
    assert.strictEqual(res.writeEarlyHints({
      link: '</styles.css>; rel=preload; as=style',
    }, common.mustCall((err) => {
      assert.strictEqual(err, null);
      order.push(103);
    })), true);
    const onInformation = common.mustCall((err) => {
      assert.strictEqual(err, null);
      order.push(110);
      assert.deepStrictEqual(order, [100, 103, 110]);
      res.end('done');
    });
    assert.strictEqual(
      res.writeInformation(110, { 'X-Progress': '50%' }, onInformation), true);
    sync = false;
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    const seen = [];
    req.on('headers', (headers) => seen.push(headers[':status']));
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      assert.deepStrictEqual(seen, [100, 103, 110]);
    }));

    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// The headers can be omitted, with or without a callback.
{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    const order = [];

    assert.strictEqual(res.writeInformation(102), true);
    assert.strictEqual(res.writeInformation(102, { 'X-Call': 'headers' }), true);
    assert.strictEqual(res.writeInformation(
      102, { 'X-Call': 'headers-callback' }, common.mustCall((err) => {
        assert.strictEqual(err, null);
        order.push('headers-callback');
      })), true);
    assert.strictEqual(res.writeInformation(
      102, undefined, common.mustCall((err) => {
        assert.strictEqual(err, null);
        order.push('undefined-callback');
        assert.deepStrictEqual(
          order, ['headers-callback', 'undefined-callback']);
        res.end('done');
      })), true);
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    const seen = [];
    req.on('headers', (headers) => {
      seen.push([headers[':status'], headers['x-call']]);
    });
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      assert.deepStrictEqual(seen, [
        [102, undefined],
        [102, 'headers'],
        [102, 'headers-callback'],
        [102, undefined],
      ]);
    }));

    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// The callback is not called when nothing is sent, and a non-function
// callback is rejected.
{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    for (const cb of [null, 1, 'cb', {}]) {
      assert.throws(() => res.writeContinue(cb),
                    { code: 'ERR_INVALID_ARG_TYPE' });
      assert.throws(() => res.writeInformation(110, undefined, cb),
                    { code: 'ERR_INVALID_ARG_TYPE' });
      assert.throws(() => res.writeEarlyHints({ link: '</a>' }, cb),
                    { code: 'ERR_INVALID_ARG_TYPE' });
    }

    res.writeHead(200);
    assert.strictEqual(res.writeContinue(common.mustNotCall()), false);
    assert.strictEqual(
      res.writeInformation(110, undefined, common.mustNotCall()), false);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}
