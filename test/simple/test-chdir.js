var common = require('../common');
var assert = require('assert');

assert.equal(true, process.cwd() !== __dirname);

process.chdir(__dirname);

assert.equal(true, process.cwd() === __dirname);
