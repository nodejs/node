// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/js-loader.mjs
/* eslint-disable required-modules */
import { namedExport } from '../fixtures/es-module-loaders/js-as-esm.js';
import assert from 'assert';
import ok from './test-esm-ok.mjs';

assert(ok);
assert(namedExport);
