'use strict';

// These testcases are specific to one uncommon behaviour in path module. Few
// of the functions in path module, treat '' strings as current working
// directory. This test makes sure that the behaviour is intact between commits.
// See: https://github.com/nodejs/node/pull/2106

require('../common');
const assert = require('assert');
const path = require('path');
const pwd = process.cwd();

// join will internally ignore all the zero-length strings and it will return
// '.' if the joined string is a zero-length string.
assert.equal(path.posix.join(''), '.');
assert.equal(path.posix.join('', ''), '.');
assert.equal(path.win32.join(''), '.');
assert.equal(path.win32.join('', ''), '.');
assert.equal(path.join(pwd), pwd);
assert.equal(path.join(pwd, ''), pwd);

// normalize will return '.' if the input is a zero-length string
assert.equal(path.posix.normalize(''), '.');
assert.equal(path.win32.normalize(''), '.');
assert.equal(path.normalize(pwd), pwd);

// Since '' is not a valid path in any of the common environments, return false
assert.equal(path.posix.isAbsolute(''), false);
assert.equal(path.win32.isAbsolute(''), false);

// resolve, internally ignores all the zero-length strings and returns the
// current working directory
assert.equal(path.resolve(''), pwd);
assert.equal(path.resolve('', ''), pwd);

// relative, internally calls resolve. So, '' is actually the current directory
assert.equal(path.relative('', pwd), '');
assert.equal(path.relative(pwd, ''), '');
assert.equal(path.relative(pwd, pwd), '');
