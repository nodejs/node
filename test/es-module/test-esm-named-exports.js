// Flags: --import ./test/fixtures/es-module-loaders/builtin-named-exports.mjs
'use strict';

const common = require('../common');
common.skipIfWorker();

const { readFile, __fromLoader } = require('fs');
const assert = require('assert');

assert.throws(() => require('../fixtures/es-modules/test-esm-ok.mjs'), { code: 'ERR_REQUIRE_ESM' });

assert(readFile);
assert(__fromLoader);
