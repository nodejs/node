/**
 * Runs a collection of tests that determine if an API implements structured clone
 * correctly.
 *
 * The `runner` parameter has the following properties:
 * - `setup()`: An optional function run once before testing starts
 * - `teardown()`: An option function run once after all tests are done
 * - `preTest()`: An optional, async function run before a test
 * - `postTest()`: An optional, async function run after a test is done
 * - `structuredClone(obj, transferList)`: Required function that somehow
 *                                         structurally clones an object.
 *                                         Must return a promise.
 * - `hasDocument`: When true, disables tests that require a document. True by default.
 */

function runStructuredCloneBatteryOfTests(runner) {
  const defaultRunner = {
    setup() {},
    preTest() {},
    postTest() {},
    teardown() {},
    hasDocument: true
  };
  runner = Object.assign({}, defaultRunner, runner);

  let setupPromise = runner.setup();
  const allTests = structuredCloneBatteryOfTests.map(test => {

    if (!runner.hasDocument && test.requiresDocument) {
      return;
    }

    return new Promise(resolve => {
      promise_test(async t => {
        test = await test;
        await setupPromise;
        await runner.preTest(test);
        await test.f(runner, t)
        await runner.postTest(test);
        resolve();
      }, test.description);
    }).catch(_ => {});
  });
  Promise.all(allTests).then(_ => runner.teardown());
}
