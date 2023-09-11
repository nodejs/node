'use strict';

// Verify that exceptions from a callback don't result in
// failed CHECKs when trying to print the exception message.

// This test is convoluted because it needs to trigger a callback
// into JS land at just the right time when an exception is pending,
// and does so by exploiting a weakness in the streams infrastructure.
// I won't shed any tears if this test ever becomes invalidated.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.argv[2] === 'child') {
  const fixtures = require('../common/fixtures');
  const https = require('https');
  const net = require('net');
  const tls = require('tls');
  const { Duplex } = require('stream');
  const { mustCall } = common;

  const cert = fixtures.readKey('rsa_cert.crt');
  const key = fixtures.readKey('rsa_private.pem');

  net.createServer(mustCall(onplaintext)).listen(0, mustCall(onlisten));

  function onlisten() {
    const { port } = this.address();
    https.get({ port, rejectUnauthorized: false });
  }

  function onplaintext(c) {
    const d = new class extends Duplex {
      _read(n) {
        const data = c.read(n);
        if (data) d.push(data);
      }
      _write(...xs) {
        c.write(...xs);
      }
    }();
    c.on('data', d.push.bind(d));

    const options = { key, cert };
    const fail = () => { throw new Error('eyecatcher'); };
    tls.createServer(options, mustCall(fail)).emit('connection', d);
  }
} else {
  const assert = require('assert');
  const { spawnSync } = require('child_process');
  const result = spawnSync(process.execPath, [__filename, 'child']);
  const stderr = result.stderr.toString();
  const ok = stderr.includes('Error: eyecatcher');
  assert(ok, stderr);
}
