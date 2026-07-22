'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const assert = require('assert');
const tls = require('tls');

const server = new tls.Server();

[null, undefined, 0, 1, 1n, Symbol(), {}, [], true, false, '', () => {}]
  .forEach((arg) =>
    assert.throws(
      () => server.setTicketKeys(arg),
      { code: 'ERR_INVALID_ARG_TYPE' }
    ));

[new Uint8Array(1), Buffer.from([1]), new DataView(new ArrayBuffer(2))].forEach(
  (arg) =>
    assert.throws(() => {
      server.setTicketKeys(arg);
    }, /Session ticket keys must be a 48-byte buffer/)
);
