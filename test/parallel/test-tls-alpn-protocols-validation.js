'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

// Array with empty string should throw (zero-length protocol entry)
assert.throws(() => {
  const out = {};
  tls.convertALPNProtocols([''], out);
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Array with empty string mixed
assert.throws(() => {
  const out = {};
  tls.convertALPNProtocols(['h2', ''], out);
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Buffer wire format with leading zero length
assert.throws(() => {
  const out = {};
  tls.convertALPNProtocols(Buffer.from([0]), out);
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Buffer truncated (claims 2 bytes but only 1 follows)
assert.throws(() => {
  const out = {};
  tls.convertALPNProtocols(Buffer.from([2, 0x61]), out);
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Buffer with trailing invalid byte
assert.throws(() => {
  const out = {};
  tls.convertALPNProtocols(Buffer.from([1, 0x61, 0x62, 0x62]), out);
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Empty array means skip ALPN (allowed)
{
  const out = {};
  tls.convertALPNProtocols([], out);
  assert.ok(Buffer.isBuffer(out.ALPNProtocols));
  assert.strictEqual(out.ALPNProtocols.length, 0);
}

// Empty buffer means skip ALPN (allowed; same as [])
{
  const out = {};
  tls.convertALPNProtocols(Buffer.alloc(0), out);
  assert.ok(Buffer.isBuffer(out.ALPNProtocols));
  assert.strictEqual(out.ALPNProtocols.length, 0);
}

// Empty Uint8Array means skip ALPN
{
  const out = {};
  tls.convertALPNProtocols(new Uint8Array(0), out);
  assert.ok(Buffer.isBuffer(out.ALPNProtocols));
  assert.strictEqual(out.ALPNProtocols.length, 0);
}

// Valid inputs should not throw
{
  const out = {};
  tls.convertALPNProtocols(['h2', 'http/1.1'], out);
  assert.ok(out.ALPNProtocols.length > 0);
}
{
  const out = {};
  tls.convertALPNProtocols(Buffer.from([
    2, 0x61, 0x62, 8, 0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31,
  ]), out);
  assert.strictEqual(out.ALPNProtocols.length, 12);
}
