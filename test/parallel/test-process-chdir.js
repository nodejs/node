'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

assert.notStrictEqual(process.cwd(), __dirname);
process.chdir(__dirname);
assert.strictEqual(process.cwd(), __dirname);

const dir = path.resolve(common.tmpDir,
    'weird \uc3a4\uc3ab\uc3af characters \u00e1\u00e2\u00e3');

// Make sure that the tmp directory is clean
common.refreshTmpDir();

fs.mkdirSync(dir);
process.chdir(dir);
assert.strictEqual(process.cwd(), dir);

process.chdir('..');
assert.strictEqual(process.cwd(), path.resolve(common.tmpDir));

assert.throws(function() { process.chdir({}); }, TypeError, 'Bad argument.');
assert.throws(function() { process.chdir(); }, TypeError, 'Bad argument.');
assert.throws(function() { process.chdir('x', 'y'); },
              TypeError, 'Bad argument.');
