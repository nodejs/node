// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const util = require('internal/util');

if (!process.versions.openssl) {
  assert.throws(
    () => util.assertCrypto(),
    /^Error: Node\.js is not compiled with openssl crypto support$/
  );
} else {
  assert.doesNotThrow(() => util.assertCrypto());
}
