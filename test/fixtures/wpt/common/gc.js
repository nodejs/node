/**
 * Does a best-effort attempt at invoking garbage collection. Attempts to use
 * the standardized `TestUtils.gc()` function, but falls back to other
 * environment-specific nonstandard functions, with a final result of just
 * creating a lot of garbage (in which case you will get a console warning).
 *
 * This should generally only be used to attempt to trigger bugs and crashes
 * inside tests, i.e. cases where if garbage collection happened, then this
 * should not trigger some misbehavior. You cannot rely on garbage collection
 * successfully trigger, or that any particular unreachable object will be
 * collected.
 *
 * @returns {Promise<undefined>} A promise you should await to ensure garbage
 * collection has had a chance to complete.
 */
self.garbageCollect = async () => {
  // https://testutils.spec.whatwg.org/#the-testutils-namespace
  if (self.TestUtils?.gc) {
    return TestUtils.gc();
  }

  // Use --expose_gc for V8 (and Node.js)
  // to pass this flag at chrome launch use: --js-flags="--expose-gc"
  // Exposed in SpiderMonkey shell as well
  if (self.gc) {
    return self.gc();
  }

  // Present in some WebKit development environments
  if (self.GCController) {
    return GCController.collect();
  }

  console.warn(
    'Tests are running without the ability to do manual garbage collection. ' +
    'They will still work, but coverage will be suboptimal.');

  for (var i = 0; i < 1000; i++) {
    gcRec(10);
  }

  function gcRec(n) {
    if (n < 1) {
      return {};
    }

    let temp = { i: "ab" + i + i / 100000 };
    temp += "foo";

    gcRec(n - 1);
  }
};
