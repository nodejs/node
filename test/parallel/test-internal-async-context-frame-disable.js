// Flags: --expose-internals --no-async-context-frame
'use strict';

require('../common');
const assert = require('assert');
const AsyncContextFrame = require('internal/async_context_frame');

// Test that AsyncContextFrame can be disabled.
assert(!AsyncContextFrame.enabled);
