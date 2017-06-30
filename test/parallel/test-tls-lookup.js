'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

const expectedError = /^TypeError: "lookup" option should be a function$/;

['foobar', 1, {}, []].forEach(function connectThrows(input) {
  const opts = {
    host: 'localhost',
    port: common.PORT,
    lookup: input
  };

  assert.throws(function() {
    tls.connect(opts);
  }, expectedError);
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
