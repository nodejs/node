// Flags: --import ./test/fixtures/es-module-loaders/builtin-named-exports.mjs
import { skipIfWorker } from '../common/index.mjs';
import * as fs from 'fs';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';
skipIfWorker();

assert(ok);
assert(fs.readFile);
assert(fs.__fromLoader);
