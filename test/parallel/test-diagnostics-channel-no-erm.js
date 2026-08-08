// Flags: --no-js-explicit-resource-management
'use strict';

require('../common');
const { tracingChannel } = require('diagnostics_channel');

// This test ensures that using diagnostics_channel does not cause a segfault
// when explicit resource management is disabled in V8.
// Refs: https://github.com/nodejs/node/issues/64230

tracingChannel('foo').traceSync(() => {});
