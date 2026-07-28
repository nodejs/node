import '../common/index.mjs';
import assert from 'assert';
// ./test-esm-ok.mjs
import ok from '../fixtures/es-modules/test-%65%73%6d-ok.mjs';
// ./test-esm-comma,.mjs
import comma from '../fixtures/es-modules/test-esm-comma%2c.mjs';

assert(ok);
assert(comma);
