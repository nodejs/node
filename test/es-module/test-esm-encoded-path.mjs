// Flags: --experimental-modules
import '../common';
import assert from 'assert';
// ./test-esm-ok.mjs
import ok from '../fixtures/es-modules/test-%65%73%6d-ok.mjs';

assert(ok);
