// Flags: --experimental-loader ./test/fixtures/es-module-loaders/example-loader.mjs
/* eslint-disable node-core/require-common-first, node-core/required-modules */
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';

assert(ok);
