// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/example-loader.mjs
/* eslint-disable node-core/required-modules */
import assert from 'assert';
import ok from './test-esm-ok.mjs';

assert(ok);
