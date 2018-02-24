// Flags: --experimental-modules --harmony-dynamic-import

import { crashOnUnhandledRejection } from '../common';
import assert from 'assert';

crashOnUnhandledRejection();

import { add } from '../fixtures/es-modules/add.wasm';

assert.strictEqual(add(2, 3), 5);

import('../fixtures/es-modules/add.wasm').then((ns) => {
  assert.strictEqual(ns.add(2, 3), 5);
  assert.strictEqual(ns.default(2), 12);
});
