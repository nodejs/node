// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/loader-shared-dep.mjs
import '../common';
import assert from 'assert';
import '../fixtures/es-modules/test-esm-ok.mjs';
import dep from '../fixtures/es-module-loaders/loader-dep.js';

assert.strictEqual(dep.format, 'esm');
