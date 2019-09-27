// Flags: --experimental-modules --experimental-loader ./test/fixtures/es-module-loaders/transform-loader.mjs
/* eslint-disable node-core/require-common-first, node-core/required-modules */
import {
  foo,
  bar
} from '../fixtures/es-module-loaders/module-named-exports.mjs';
import assert from 'assert';

assert.strictEqual(foo, 'transformed-foo');
assert.strictEqual(bar, 'transformed-bar');
