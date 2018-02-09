'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

['foobar', 1, {}, []].forEach(function connectThrows(input) {
  const opts = {
    host: 'localhost',
    port: common.PORT,
    lookup: input
  };

  common.expectsError(function() {
    tls.connect(opts);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });
});

connectDoesNotThrow(common.mustCall(() => {}));

function connectDoesNotThrow(input) {
  const opts = {
    host: 'localhost',
    port: common.PORT,
    lookup: input
  };

  tls.connect(opts);
}
