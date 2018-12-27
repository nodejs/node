import { requireFlags } from '../common';
import assert from 'assert';
import '../fixtures/es-modules/test-esm-ok.mjs';
import dep from '../fixtures/es-module-loaders/loader-dep.js';

const flag = '--loader=./test/fixtures/es-module-loaders/loader-shared-dep.mjs';
if (!process.execArgv.includes(flag)) {
  // Include `--experimental-modules` explicitly for workers.
  requireFlags(['--experimental-modules', flag]);
}

assert.strictEqual(dep.format, 'esm');
