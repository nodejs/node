'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

assert.notStrictEqual(process.cwd(), __dirname);
process.chdir(__dirname);
assert.strictEqual(process.cwd(), __dirname);

let dirName;
if (process.versions.icu) {
  // ICU is available, use characters that could possibly be decomposed
  dirName = 'weird \uc3a4\uc3ab\uc3af characters \u00e1\u00e2\u00e3';
} else {
  // ICU is unavailable, use characters that can't be decomposed
  dirName = 'weird \ud83d\udc04 characters \ud83d\udc05';
}
const dir = path.resolve(common.tmpDir, dirName);

// Make sure that the tmp directory is clean
common.refreshTmpDir();

fs.mkdirSync(dir);
process.chdir(dir);
assert.strictEqual(process.cwd().normalize(), dir.normalize());

process.chdir('..');
assert.strictEqual(process.cwd().normalize(),
                   path.resolve(common.tmpDir).normalize());

assert.throws(function() { process.chdir({}); }, TypeError, 'Bad argument.');
assert.throws(function() { process.chdir(); }, TypeError, 'Bad argument.');
assert.throws(function() { process.chdir('x', 'y'); },
              TypeError, 'Bad argument.');
