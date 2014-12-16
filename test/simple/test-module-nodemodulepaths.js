// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var module = require('module');

var isWindows = process.platform === 'win32';

var file, delimiter, paths, expected_paths, old_home;

if (isWindows) {
  file = 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo';
  delimiter = '\\'
  expected_paths = [
    'C:\\Users\\Rocko Artischocko\\node_stuff\\foo\\node_modules',
    'C:\\Users\\Rocko Artischocko\\node_stuff\\node_modules',
    'C:\\Users\\Rocko Artischocko\\node_modules'
  ];
  old_home = process.env.USERPROFILE;
  process.env.USERPROFILE = 'C:\\Users\\Rocko Artischocko';
} else {
  file = '/usr/test/lib/node_modules/npm/foo';
  delimiter = '/'
  expected_paths = [
    '/usr/test/lib/node_modules/npm/foo/node_modules',
    '/usr/test/lib/node_modules/npm/node_modules',
    '/usr/test/lib/node_modules',
    '/usr/test/node_modules'
  ];
  old_home = process.env.HOME;
  process.env.HOME = '/usr/test';
}

paths = module._nodeModulePaths(file);

assert.ok(paths.indexOf(file + delimiter + 'node_modules') !== -1);
assert.deepEqual(expected_paths, paths);
assert.ok(Array.isArray(paths));

// Restore mocked home directory environment variables for other tests
if (isWindows) {
  process.env.USERPROFILE = old_home;
} else {
  process.env.HOME = old_home;
}
