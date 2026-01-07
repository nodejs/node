'use strict';
// Addons: test_finalizer, test_finalizer_vtable

const common = require('../../common');
const { addonPath, isInvokedAsChild, spawnTestSync } =
  require('../../common/addon-test');
const { gcUntil } = require('../../common/gc');
const assert = require('assert');

if (isInvokedAsChild) {
  const test_finalizer = require(addonPath);

  (() => {
    const obj = {};
    test_finalizer.addFinalizerFailOnJS(obj);
  })();

  // Collect garbage 10 times. At least one of those should throw the exception
  // and cause the whole process to bail with it, its text printed to stderr and
  // asserted by the parent process to match expectations.
  gcUntil('fatal finalize', () => false);
} else {
  const child = spawnTestSync(['--expose-gc']);
  assert(common.nodeProcessAborted(child.status, child.signal));
  assert.match(child.stderr.toString(), /Finalizer is calling a function that may affect GC state/);
}
