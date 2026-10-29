// Flags: --import ./test/fixtures/es-module-loaders/builtin-named-exports.mjs
import '../common/index.mjs';
import { readFile, __fromLoader } from 'fs';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';

assert(ok);
assert(readFile);
assert(__fromLoader);
