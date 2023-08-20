'use strict';
require('../common');

// This tests Module._stat.

const Module = require('module');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { ok, strictEqual } = require('assert');

const directory = tmpdir.resolve('directory');
const doesNotExist = tmpdir.resolve('does-not-exist');
const file = tmpdir.resolve('file.js');

tmpdir.refresh();
fs.writeFileSync(file, "module.exports = { a: 'b' }");
fs.mkdirSync(directory);

strictEqual(Module._stat(directory), 1); // Returns 1 for directories.
strictEqual(Module._stat(file), 0); // Returns 0 for files.
ok(Module._stat(doesNotExist) < 0); // Returns a negative integer for any other kind of strings.

// TODO(RaisinTen): Add tests that make sure that Module._stat() does not crash when called
// with a non-string data type. It crashes currently.
