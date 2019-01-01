import { requireFlags } from '../common';
import assert from 'assert';
import '../fixtures/es-modules/test-esm-ok.mjs';
import dep from '../fixtures/es-module-loaders/loader-dep.js';

requireFlags(
  '--experimental-modules',
  '--loader=./test/fixtures/es-module-loaders/loader-shared-dep.mjs'
);

assert.strictEqual(dep.format, 'esm');
