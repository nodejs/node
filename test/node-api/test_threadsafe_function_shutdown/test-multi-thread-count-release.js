'use strict';

const common = require('../../common');
const run = require(`./build/${common.buildType}/multi_thread_count_release`);

// Ensures that the finalizer is called exactly once regardless of initial thread count
// of a TSFN.
// Also this verifies that multiple thread count does not cause re-entrant finalization.

run(false, common.mustCall()); // Three releases.
run(true, common.mustCall()); // One abort, then two calls returning napi_closing.
