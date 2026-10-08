'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const { test } = require('node:test');
const { assertAborts } = require('./ffi-callback-test-common');

test('ffi aborts on cross-thread callback invocation', () => {
  const workerSource = `
const { workerData } = require('node:worker_threads');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require(${JSON.stringify(require.resolve('./ffi-test-common'))});
const { functions } = ffi.dlopen(libraryPath, fixtureSymbols);
functions.call_int_callback(workerData, 21);
`;
  assertAborts(
    `'use strict';
const { Worker } = require('node:worker_threads');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require(${JSON.stringify(require.resolve('./ffi-test-common'))});
const { lib } = ffi.dlopen(libraryPath, fixtureSymbols);
const callback = lib.registerCallback(
  { arguments: ['i32'], return: 'i32' },
  (value) => value * 2,
);
new Worker(${JSON.stringify(workerSource)}, { eval: true, workerData: callback });`,
    /Callbacks can only be invoked on the system thread they were created on/,
  );
});
