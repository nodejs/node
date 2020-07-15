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
  const interval = setInterval(() => {
    clearInterval(interval);
  }, 100);
  return;
}

const assert = require('assert');
const { spawnSync } = require('child_process');
const child = spawnSync(process.execPath, [ __filename, 'child' ]);
assert.strictEqual(child.signal, null);
assert.match(child.stderr.toString(), /Error during Finalize/m);
