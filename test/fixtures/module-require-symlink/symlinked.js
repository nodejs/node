'use strict';
const common = require('../../common');
const assert = require('assert');
const foo = require('./foo');
const path = require('path');

const linkScriptTarget = path.join(common.fixturesDir,
  'module-require-symlink', 'symlinked.js');

assert.strictEqual(foo.dep1.bar.version, 'CORRECT_VERSION');
assert.strictEqual(foo.dep2.bar.version, 'CORRECT_VERSION');
assert.strictEqual(__filename, linkScriptTarget);
assert(__filename in require.cache);
