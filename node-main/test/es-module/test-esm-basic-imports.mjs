import '../common/index.mjs';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';
import * as okShebangNs from './test-esm-shebang.mjs';
// encode the '.'
import * as okShebangPercentNs from './test-esm-shebang%2emjs';

assert(ok);
assert(okShebangNs.default);
assert.strict.equal(okShebangNs, okShebangPercentNs);
