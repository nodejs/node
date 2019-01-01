import { requireFlags } from '../common';
import { readFile } from 'fs';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';

requireFlags(
  '--experimental-modules',
  '--loader=./test/fixtures/es-module-loaders/builtin-named-exports-loader.mjs'
);

assert(ok);
assert(readFile);
