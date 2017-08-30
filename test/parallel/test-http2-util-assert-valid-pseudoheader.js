// Flags: --expose-internals
'use strict';

const common = require('../common');

// Tests the assertValidPseudoHeader function that is used within the
// mapToHeaders function. The assert function is not exported so we
// have to test it through mapToHeaders

const { mapToHeaders } = require('internal/http2/util');
const assert = require('assert');

function isNotError(val) {
  assert(!(val instanceof Error));
}

function isError(val) {
  common.expectsError({
    code: 'ERR_HTTP2_INVALID_PSEUDOHEADER',
    type: Error,
    message: '":foo" is an invalid pseudoheader or is used incorrectly'
  })(val);
}

isNotError(mapToHeaders({ ':status': 'a' }));
isNotError(mapToHeaders({ ':path': 'a' }));
isNotError(mapToHeaders({ ':authority': 'a' }));
isNotError(mapToHeaders({ ':scheme': 'a' }));
isNotError(mapToHeaders({ ':method': 'a' }));

isError(mapToHeaders({ ':foo': 'a' }));
