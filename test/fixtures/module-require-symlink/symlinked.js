'use strict';
const assert = require('assert');
const common = require('../../common');
const path = require('path');

const linkScriptTarget = path.join(common.fixturesDir,
  '/module-require-symlink/symlinked.js');

var foo = require('./foo');
assert.equal(foo.dep1.bar.version, 'CORRECT_VERSION');
assert.equal(foo.dep2.bar.version, 'CORRECT_VERSION');
assert.equal(__filename, linkScriptTarget);
assert(__filename in require.cache);
