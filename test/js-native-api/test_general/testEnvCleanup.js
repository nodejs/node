'use strict';

if (process.argv[2] === 'child') {
  const common = require('../../common');
  const test_general = require(`./build/${common.buildType}/test_general`);

  // The second argument to `envCleanupWrap()` is an index into the global
  // static string array named `env_cleanup_finalizer_messages` on the native
  // side. A reverse mapping is reproduced here for clarity.
  const finalizerMessages = {
    'simple wrap': 0,
    'wrap, removeWrap': 1,
    'first wrap': 2,
    'second wrap': 3
  };

  // We attach the three objects we will test to `module.exports` to ensure they
  // will not be garbage-collected before the process exits.

  // Make sure the finalizer for a simple wrap will be called at env cleanup.
  module.exports['simple wrap'] =
    test_general.envCleanupWrap({}, finalizerMessages['simple wrap']);

  // Make sure that a removed wrap does not result in a call to its finalizer at
  // env cleanup.
  module.exports['wrap, removeWrap'] =
    test_general.envCleanupWrap({}, finalizerMessages['wrap, removeWrap']);
  test_general.removeWrap(module.exports['wrap, removeWrap']);

  // Make sure that only the latest attached version of a re-wrapped item's
  // finalizer gets called at env cleanup.
  module.exports['first wrap'] =
    test_general.envCleanupWrap({}, finalizerMessages['first wrap']),
  test_general.removeWrap(module.exports['first wrap']);
  test_general.envCleanupWrap(module.exports['first wrap'],
                              finalizerMessages['second wrap']);
} else {
  const assert = require('assert');
  const { spawnSync } = require('child_process');

  const child = spawnSync(process.execPath, [__filename, 'child'], {
    stdio: [ process.stdin, 'pipe', process.stderr ]
  });

  // Grab the child's output and construct an object whose keys are the rows of
  // the output and whose values are `true`, so we can compare the output while
  // ignoring the order in which the lines of it were produced.
  assert.deepStrictEqual(
    child.stdout.toString().split(/\r\n|\r|\n/g).reduce((obj, item) =>
      Object.assign(obj, item ? { [item]: true } : {}), {}), {
      'finalize at env cleanup for simple wrap': true,
      'finalize at env cleanup for second wrap': true
    });

  // Ensure that the child exited successfully.
  assert.strictEqual(child.status, 0);
}
