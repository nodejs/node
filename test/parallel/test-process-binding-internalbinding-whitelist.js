// Flags: --no-warnings
'use strict';

require('../common');
const assert = require('assert');

// Assert that whitelisted internalBinding modules are accessible via
// process.binding().
assert(process.binding('uv'));
assert(process.binding('http_parser'));
assert(process.binding('v8'));
assert(process.binding('stream_wrap'));
assert(process.binding('signal_wrap'));
assert(process.binding('contextify'));
