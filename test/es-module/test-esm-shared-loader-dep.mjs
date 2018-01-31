// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/loader-shared-dep.mjs
/* eslint-disable required-modules */
import assert from 'assert';
import './test-esm-ok.mjs';
import dep from '../fixtures/es-module-loaders/loader-dep.js';

assert.strictEqual(dep.format, 'esm');
