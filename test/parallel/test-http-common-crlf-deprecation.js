// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

common.expectWarning(
  'DeprecationWarning',
  "The CRLF constant from '_http_common' is deprecated. " +
  "Use '\\r\\n' directly instead.",
  'DEP0205',
);

const { CRLF } = require('_http_common');

assert.strictEqual(CRLF, '\r\n');

// Accessing CRLF again should not emit another warning
assert.strictEqual(CRLF, '\r\n');
