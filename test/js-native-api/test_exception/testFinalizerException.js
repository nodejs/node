'use strict';
// Addons: test_exception, test_exception_vtable

const { addonPath, isInvokedAsChild, spawnTestSync } =
  require('../../common/addon-test');
const { gcUntil } = require('../../common/gc');
const assert = require('assert');

if (isInvokedAsChild) {
  // Trying, catching the exception, and finding the bindings at the `Error`'s
  // `binding` property is done intentionally, because we're also testing what
  // happens when the add-on entry point throws. See test.js.
  try {
    require(addonPath);
  } catch (anException) {
    anException.binding.createExternal();
  }

  // Collect garbage 10 times. At least one of those should throw the exception
  // and cause the whole process to bail with it, its text printed to stderr and
  // asserted by the parent process to match expectations.
  gcUntil('finalizer exception', () => false);
} else {
  const child = spawnTestSync(['--expose-gc']);
  assert.strictEqual(child.signal, null);
  assert.match(child.stderr.toString(), /Error during Finalize/);
}
