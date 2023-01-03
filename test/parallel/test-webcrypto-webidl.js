// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const webidl = require('internal/crypto/webidl');

const prefix = "Failed to execute 'fn' on 'interface'";

{
  assert.throws(() => webidl.requiredArguments(0, 3, { prefix }), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: "Failed to execute 'fn' on 'interface': 3 arguments required, but only 0 present."
  });

  assert.throws(() => webidl.requiredArguments(0, 1, { prefix }), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: "Failed to execute 'fn' on 'interface': 1 argument required, but only 0 present."
  });

  // Does not throw when extra are added
  webidl.requiredArguments(4, 3, { prefix });
}
