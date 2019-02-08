// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/import-meta-loader.mjs
/* eslint-disable node-core/required-modules */
import assert from 'assert';

assert.strictEqual(import.meta.custom, 'custom-meta');
