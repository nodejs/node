'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

['foobar', 1, {}, []].forEach(function connectThrows(input) {
  const opts = {
    host: 'localhost',
    port: common.PORT,
    lookup: input
  };

  assert.throws(function() {
    tls.connect(opts);
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  }));
});

connectDoesNotThrow(common.mustCall(() => {}));

function connectDoesNotThrow(input) {
  const opts = {
    host: 'localhost',
    port: common.PORT,
    lookup: input
  };

  assert.doesNotThrow(function() {
    tls.connect(opts);
  });
}
