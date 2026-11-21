'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Reading or writing tls.CLIENT_RENEG_LIMIT or tls.CLIENT_RENEG_WINDOW emits
// a deprecation warning. Copying the module's exports or importing it as an
// ES module does not, as the properties are not enumerable.

const assert = require('assert');
const tls = require('tls');

const unexpectedWarning = common.mustNotCall('warned without access');
process.on('warning', unexpectedWarning);
assert.strictEqual(Object.keys(tls).includes('CLIENT_RENEG_LIMIT'), false);
assert.strictEqual(Object.keys(tls).includes('CLIENT_RENEG_WINDOW'), false);

import('node:tls').then(async (namespace) => {
  assert.strictEqual(namespace.CLIENT_RENEG_LIMIT, undefined);
  assert.strictEqual(namespace.default, tls);
  // Warnings are emitted on a later tick.
  await new Promise(setImmediate);
  process.off('warning', unexpectedWarning);
  common.expectWarning(
    'DeprecationWarning',
    'tls.CLIENT_RENEG_LIMIT and tls.CLIENT_RENEG_WINDOW are deprecated, as ' +
    'TLS renegotiation is deprecated.',
    'DEP0213');

  assert.strictEqual(tls.CLIENT_RENEG_LIMIT, 3);
  assert.strictEqual(tls.CLIENT_RENEG_WINDOW, 600);
  tls.CLIENT_RENEG_LIMIT = 1;
  tls.CLIENT_RENEG_WINDOW = 10;
  assert.strictEqual(tls.CLIENT_RENEG_LIMIT, 1);
  assert.strictEqual(tls.CLIENT_RENEG_WINDOW, 10);
}).then(common.mustCall());
