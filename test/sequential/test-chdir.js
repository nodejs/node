'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var path = require('path');

assert.equal(true, process.cwd() !== __dirname);

process.chdir(__dirname);
assert.equal(true, process.cwd() === __dirname);

var dir = path.resolve(common.fixturesDir,
    'weird \uc3a4\uc3ab\uc3af characters \u00e1\u00e2\u00e3');
fs.mkdirSync(dir);
process.chdir(dir);
assert(process.cwd() == dir);

process.chdir('..');
assert(process.cwd() == path.resolve(common.fixturesDir));
fs.rmdirSync(dir);

assert.throws(function() { process.chdir({}); }, TypeError, 'Bad argument.');
assert.throws(function() { process.chdir(); }, TypeError, 'Bad argument.');
assert.throws(function() { process.chdir('x', 'y'); },
              TypeError, 'Bad argument.');
