'use strict';
const common = require('../../common');
const bindingPath = require.resolve(`./build/${common.buildType}/binding`);
const binding = require(bindingPath);

// This tests checks that addons may leak libuv handles until process exit.
// It’s really not a good idea to do so, but it tests existing behaviour
// that likely can never be removed for backwards compatibility.

// This has a sibling test in test/abort/ which checks output for failures
// from workers.

try {
  // We don’t want to run this in Workers because they do actually enforce
  // a clean-exit policy.
  const { isMainThread } = require('worker_threads');
  if (!isMainThread)
    common.skip('Cannot run test in environment with clean-exit policy');
} catch {}

binding.leakHandle();
binding.leakHandle(0);
binding.leakHandle(1);
