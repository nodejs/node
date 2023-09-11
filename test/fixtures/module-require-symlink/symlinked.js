'use strict';
const assert = require('assert');
const path = require('path');

const foo = require('./foo');

const linkScriptEnding = path.join('module-require-symlink', 'symlinked.js');

assert.strictEqual(foo.dep1.bar.version, 'CORRECT_VERSION');
assert.strictEqual(foo.dep2.bar.version, 'CORRECT_VERSION');
assert(__filename.endsWith(linkScriptEnding));
assert(__filename in require.cache);
