// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');

// Tests the assertValidPseudoHeader function that is used within the
// buildNgHeaderString function. The assert function is not exported so we
// have to test it through buildNgHeaderString

const { buildNgHeaderString } = require('internal/http2/util');

// These should not throw
buildNgHeaderString({ ':status': 'a' });
buildNgHeaderString({ ':path': 'a' });
buildNgHeaderString({ ':authority': 'a' });
buildNgHeaderString({ ':scheme': 'a' });
buildNgHeaderString({ ':method': 'a' });

assert.throws(() => buildNgHeaderString({ ':foo': 'a' }), {
  code: 'ERR_HTTP2_INVALID_PSEUDOHEADER',
  name: 'TypeError',
  message: '":foo" is an invalid pseudoheader or is used incorrectly'
});
