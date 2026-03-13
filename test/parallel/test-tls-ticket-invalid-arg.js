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
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "buffer" argument must be an instance of Buffer, TypedArray, or DataView.' +
                 common.invalidArgTypeHelper(arg),
      }
    ));

[new Uint8Array(1), Buffer.from([1]), new DataView(new ArrayBuffer(2))].forEach(
  (arg) =>
    assert.throws(
      () => server.setTicketKeys(arg),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_VALUE',
        message: `The argument 'keys' must be exactly 48 bytes. Received ${arg.byteLength}`,
      }
    )
);
