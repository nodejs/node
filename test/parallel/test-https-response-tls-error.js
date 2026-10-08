'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const { Writable, pipeline } = require('stream');
const fixtures = require('../common/fixtures');

// Verify error propagation after receiving response headers. The invalid
// record deliberately causes a TLS error; it does not reproduce #66001's
// failure to decrypt an intact stream under backpressure.
for (const version of ['TLSv1.2', 'TLSv1.3']) {
  let transport;
  let requestError;
  const events = [];
  const server = https.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    minVersion: version,
    maxVersion: version,
  }, common.mustCall((req, res) => {
    res.writeHead(200, { 'Content-Length': 100 });
    res.write('partial body');
  }));
  server.on('connection', common.mustCall((socket) => {
    transport = socket;
  }));

  server.listen(0, common.mustCall(() => {
    const req = https.get({
      port: server.address().port,
      rejectUnauthorized: false,
      agent: false,
    }, common.mustCall((res) => {
      assert.strictEqual(res.complete, false);
      res.on('aborted', common.mustCall(() => events.push('aborted')));
      res.on('error', common.mustCall((err) => {
        events.push('response error');
        assert.strictEqual(err.code, 'ECONNRESET');
        assert.strictEqual(err.message, 'aborted');
        assert.strictEqual(err.cause, requestError);
      }));
      res.on('close', common.mustCall(() => {
        events.push('response close');
        assert.deepStrictEqual(events, [
          'request error',
          'aborted',
          'request close',
          'response error',
          'response close',
        ]);
      }));

      pipeline(res, new Writable({
        write(chunk, encoding, callback) {
          callback();
        },
      }), common.mustCall((err) => {
        assert.strictEqual(err, res.errored);
        assert.strictEqual(err.cause, requestError);
        assert.strictEqual(res.complete, false);
        assert.strictEqual(res.aborted, true);
        server.close(common.mustCall());
        transport.destroy();
      }));

      // Bypass the server's TLSSocket to send an invalid application-data
      // record after the client has received part of the HTTP response.
      const record = Buffer.alloc(37);
      record.set([23, 3, 3, 0, 32]);
      transport.write(record);
    }));
    req.on('error', common.mustCall((err) => {
      events.push('request error');
      assert.match(err.code, /^ERR_SSL_/);
      requestError = err;
    }));
    req.on('close', common.mustCall(() => events.push('request close')));
  }));
}
