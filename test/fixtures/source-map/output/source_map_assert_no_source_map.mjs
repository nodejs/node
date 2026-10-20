// Flags: --enable-source-maps

import '../../../common/index.mjs';
import { strict as assert } from 'node:assert';

// Regression test for https://github.com/nodejs/node/issues/63169
// Under --enable-source-maps with no source map for this file, a failing
// assert(value) must throw AssertionError, not TypeError ERR_INVALID_ARG_TYPE.
assert(false); // eslint-disable-line no-restricted-syntax
