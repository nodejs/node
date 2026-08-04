'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const {
  getHarnessErrorName,
  getUnexpectedPasses,
  isUnexpectedPass,
  WPTRunner,
  WPTTestSpec,
} = require('../common/wpt');

const flakyName = getHarnessErrorName({
  message: 'this uncaught rejection is expected',
  stack: 'Error: this uncaught rejection is expected\n    at held.https.any.js:71:14',
});
assert.strictEqual(flakyName, 'Error: this uncaught rejection is expected');
assert.strictEqual(
  getHarnessErrorName({ message: 'harness failed' }),
  'harness failed',
);

const flakySpecs = WPTTestSpec.from(
  'web-locks',
  'held.https.any.js',
  [{
    key: 'held.https.any.js',
    requires: [],
    fail: { flaky: [flakyName] },
  }],
);
assert.strictEqual(flakySpecs.length, 2);
assert.ok(flakySpecs.every((spec) =>
  spec.failedTests.includes(flakyName) && spec.flakyTests.includes(flakyName)));
assert.ok(flakySpecs.every((spec) => !isUnexpectedPass(spec, flakyName)));
assert.deepStrictEqual(getUnexpectedPasses(flakySpecs, {}), []);

const specs = WPTTestSpec.from(
  'WebCryptoAPI',
  'getPublicKey.tentative.https.any.js',
  [],
);

assert.deepStrictEqual(specs.map((spec) => ({
  testPath: spec.getTestPath(),
  isWebWorkerTest: spec.isWebWorkerTest(),
})), [
  {
    testPath: 'WebCryptoAPI/getPublicKey.tentative.https.any.html',
    isWebWorkerTest: false,
  },
  {
    testPath: 'WebCryptoAPI/getPublicKey.tentative.https.any.worker.html',
    isWebWorkerTest: true,
  },
]);

const runner = new WPTRunner('WebCryptoAPI');
assert.throws(
  () => new WPTRunner('WebCryptoAPI', { concurrency: 0 }),
  /WPT concurrency must be a positive integer/,
);
runner.pretendGlobalThisAs('Window');
assert.match(runner.fullInitScript(specs[0]), /globalThis\.Window/);
assert.doesNotMatch(runner.fullInitScript(specs[1]), /globalThis\.Window/);

const nonTestDirs = new Set(['resources', 'support', 'tools']);
const webLocksRunner = new WPTRunner('web-locks');
assert.ok([...webLocksRunner.specs].every((spec) =>
  !spec.filename.split(path.sep).some((part) => nonTestDirs.has(part))));

const variantSpecs = WPTTestSpec.from(
  'WebCryptoAPI',
  'derive_bits_keys/hkdf.https.any.js',
  [],
);

assert.strictEqual(variantSpecs.length, 8);
assert.deepStrictEqual(
  variantSpecs.filter((spec) => spec.isWebWorkerTest())
    .map((spec) => spec.getTestPath()),
  [
    'WebCryptoAPI/derive_bits_keys/hkdf.https.any.worker.html?1-1000',
    'WebCryptoAPI/derive_bits_keys/hkdf.https.any.worker.html?1001-2000',
    'WebCryptoAPI/derive_bits_keys/hkdf.https.any.worker.html?2001-3000',
    'WebCryptoAPI/derive_bits_keys/hkdf.https.any.worker.html?3001-last',
  ],
);

const skippedSpecs = WPTTestSpec.from(
  'WebCryptoAPI',
  'getPublicKey.tentative.https.any.js',
  [{ requires: [], skip: 'unsupported' }],
);
assert.deepStrictEqual(
  skippedSpecs.map(({ skipReasons }) => skipReasons),
  [['unsupported'], ['unsupported']],
);

const skippedSubtestSpecs = WPTTestSpec.from(
  'WebCryptoAPI',
  'getPublicKey.tentative.https.any.js',
  [{ requires: [], skipTests: [/getPublicKey method/] }],
);
assert.ok(skippedSubtestSpecs.every(
  (spec) => spec.isSkippedTest('getPublicKey method is available')));

const scopedSpecs = WPTTestSpec.from(
  'WebCryptoAPI',
  'getPublicKey.tentative.https.any.js',
  [],
  (spec) => (spec.getStatusKey().endsWith('.any.worker.html') ?
    [{ requires: [], skip: 'worker only' }] : []),
);
assert.deepStrictEqual(
  scopedSpecs.map(({ skipReasons }) => skipReasons),
  [[], ['worker only']],
);

const sourceRule = {
  key: 'getPublicKey.tentative.https.any.js',
  requires: [],
  fail: { expected: ['shared failure'] },
};
const workerRule = {
  key: 'getPublicKey.tentative.https.any.worker.html',
  requires: [],
  fail: { expected: ['worker failure'] },
};
const expectedFailureSpecs = WPTTestSpec.from(
  'WebCryptoAPI',
  'getPublicKey.tentative.https.any.js',
  [sourceRule],
  (spec) => (spec.isWebWorkerTest() ? [workerRule] : []),
);
const expectedResults = {
  'getPublicKey.tentative.https.any.html': {
    fail: { expected: ['shared failure'] },
  },
  'getPublicKey.tentative.https.any.worker.html': {
    fail: { expected: ['shared failure', 'worker failure'] },
  },
};

assert.deepStrictEqual(
  getUnexpectedPasses(expectedFailureSpecs, expectedResults),
  [],
);
expectedResults['getPublicKey.tentative.https.any.worker.html'].fail.expected =
  ['worker failure'];
assert.deepStrictEqual(
  getUnexpectedPasses(expectedFailureSpecs, expectedResults),
  ['getPublicKey.tentative.https.any.worker.html:shared failure'],
);
expectedResults['getPublicKey.tentative.https.any.worker.html'].fail.expected =
  ['shared failure'];
assert.deepStrictEqual(
  getUnexpectedPasses(expectedFailureSpecs, expectedResults),
  ['getPublicKey.tentative.https.any.worker.html:worker failure'],
);
