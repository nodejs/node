'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto').webcrypto;

crypto.subtle.importKey(
  'raw',
  new Uint8Array(32),
  {
    name: 'AES-GCM'
  },
  false,
  [ 'encrypt', 'decrypt' ])
  .then((k) => {
    assert.rejects(() => {
      return crypto.subtle.decrypt({
        name: 'AES-GCM',
        iv: new Uint8Array(12),
      }, k, new Uint8Array(0));
    }, {
      name: 'OperationError',
      message: /The provided data is too small/,
    });
  });
