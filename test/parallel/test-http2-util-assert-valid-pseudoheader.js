// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');

const { assertValidPseudoHeader } = require('internal/http2/util');

// These should not throw
assertValidPseudoHeader(':status');
assertValidPseudoHeader(':path');
assertValidPseudoHeader(':authority');
assertValidPseudoHeader(':scheme');
assertValidPseudoHeader(':method');

assert.throws(() => assertValidPseudoHeader(':foo'), {
  code: 'ERR_HTTP2_INVALID_PSEUDOHEADER',
  name: 'TypeError',
  message: '":foo" is an invalid pseudoheader or is used incorrectly'
});
