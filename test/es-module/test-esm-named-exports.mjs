import { relaunchWithFlags } from '../common';
import { readFile } from 'fs';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';

const flag =
  '--loader=./test/fixtures/es-module-loaders/builtin-named-exports-loader.mjs';
if (!process.execArgv.includes(flag)) {
  // Include `--experimental-modules` explicitly for workers.
  relaunchWithFlags(['--experimental-modules', flag]);
}

assert(ok);
assert(readFile);
