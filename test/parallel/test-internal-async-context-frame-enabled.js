// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const AsyncContextFrame = require('internal/async_context_frame');

// Test that AsyncContextFrame is enabled by default.
assert(AsyncContextFrame.enabled);
