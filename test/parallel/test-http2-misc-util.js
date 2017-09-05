// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');

const { sessionName } = require('internal/http2/util');

// Code coverage for sessionName utility function
assert.strictEqual(sessionName(0), 'server');
assert.strictEqual(sessionName(1), 'client');
[2, '', 'test', {}, [], true].forEach((i) => {
  assert.strictEqual(sessionName(2), '<invalid>');
});
