// Flags: --experimental-modules
import '../common';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';
import okShebang from './test-esm-shebang.mjs';

assert(ok);
assert(okShebang);
