'use strict';
if (process.argv[2] === 'child') {
  const common = require('../../common');
  // Trying, catching the exception, and finding the bindings at the `Error`'s
  // `binding` property is done intentionally, because we're also testing what
  // happens when the add-on entry point throws. See test.js.
  try {
    require(`./build/${common.buildType}/test_exception`);
  } catch (anException) {
    anException.binding.createExternal();
  }

  // Collect garbage 10 times. At least one of those should throw the exception
  // and cause the whole process to bail with it, its text printed to stderr and
  // asserted by the parent process to match expectations.
  let gcCount = 10;
  (function gcLoop() {
    global.gc();
    if (--gcCount > 0) {
      setImmediate(() => gcLoop());
    }
  })();
  return;
}

const assert = require('assert');
const { spawnSync } = require('child_process');
const child = spawnSync(process.execPath, [
  '--expose-gc', __filename, 'child',
]);
assert.strictEqual(child.signal, null);
assert.match(child.stderr.toString(), /Error during Finalize/);
