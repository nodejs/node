// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/js-loader.mjs
/* eslint-disable node-core/required-modules */
import { namedExport } from '../fixtures/es-module-loaders/js-as-esm.js';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';

assert(ok);
assert(namedExport);
