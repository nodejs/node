// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/builtin-named-exports-loader.mjs
import '../common';
import { readFile } from 'fs';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';

assert(ok);
assert(readFile);
