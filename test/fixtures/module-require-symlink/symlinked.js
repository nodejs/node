'use strict';
const assert = require('assert');
const foo = require('./foo');
const fixtures = require('../../common/fixtures');

const linkScriptTarget = fixtures.path('module-require-symlink', 'symlinked.js');

assert.strictEqual(foo.dep1.bar.version, 'CORRECT_VERSION');
assert.strictEqual(foo.dep2.bar.version, 'CORRECT_VERSION');
assert.strictEqual(__filename, linkScriptTarget);
assert(__filename in require.cache);
