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

'use strict';

const common = require('../common');
const assert = require('assert');
const _module = require('module');

const cases = {
  'WIN': [{
    file: 'C:\\Users\\hefangshi\\AppData\\Roaming\
\\npm\\node_modules\\npm\\node_modules\\minimatch',
    expect: [
      'C:\\Users\\hefangshi\\AppData\\Roaming\
\\npm\\node_modules\\npm\\node_modules\\minimatch\\node_modules',
      'C:\\Users\\hefangshi\\AppData\\Roaming\
\\npm\\node_modules\\npm\\node_modules',
      'C:\\Users\\hefangshi\\AppData\\Roaming\\npm\\node_modules',
      'C:\\Users\\hefangshi\\AppData\\Roaming\\node_modules',
      'C:\\Users\\hefangshi\\AppData\\node_modules',
      'C:\\Users\\hefangshi\\node_modules',
      'C:\\Users\\node_modules',
      'C:\\node_modules'
    ]
  }, {
    file: 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo',
    expect: [
      'C:\\Users\\Rocko Artischocko\\node_stuff\\foo\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_stuff\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_modules',
      'C:\\Users\\node_modules',
      'C:\\node_modules'
    ]
  }, {
    file: 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo_node_modules',
    expect: [
      'C:\\Users\\Rocko \
Artischocko\\node_stuff\\foo_node_modules\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_stuff\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_modules',
      'C:\\Users\\node_modules',
      'C:\\node_modules'
    ]
  }, {
    file: 'C:\\node_modules',
    expect: [
      'C:\\node_modules'
    ]
  }, {
    file: 'C:\\',
    expect: [
      'C:\\node_modules'
    ]
  }],
  'POSIX': [{
    file: '/usr/lib/node_modules/npm/node_modules/\
node-gyp/node_modules/glob/node_modules/minimatch',
    expect: [
      '/usr/lib/node_modules/npm/node_modules/\
node-gyp/node_modules/glob/node_modules/minimatch/node_modules',
      '/usr/lib/node_modules/npm/node_modules/\
node-gyp/node_modules/glob/node_modules',
      '/usr/lib/node_modules/npm/node_modules/node-gyp/node_modules',
      '/usr/lib/node_modules/npm/node_modules',
      '/usr/lib/node_modules',
      '/usr/node_modules',
      '/node_modules'
    ]
  }, {
    file: '/usr/test/lib/node_modules/npm/foo',
    expect: [
      '/usr/test/lib/node_modules/npm/foo/node_modules',
      '/usr/test/lib/node_modules/npm/node_modules',
      '/usr/test/lib/node_modules',
      '/usr/test/node_modules',
      '/usr/node_modules',
      '/node_modules'
    ]
  }, {
    file: '/usr/test/lib/node_modules/npm/foo_node_modules',
    expect: [
      '/usr/test/lib/node_modules/npm/foo_node_modules/node_modules',
      '/usr/test/lib/node_modules/npm/node_modules',
      '/usr/test/lib/node_modules',
      '/usr/test/node_modules',
      '/usr/node_modules',
      '/node_modules'
    ]
  }, {
    file: '/node_modules',
    expect: [
      '/node_modules'
    ]
  }, {
    file: '/',
    expect: [
      '/node_modules'
    ]
  }]
};

const platformCases = common.isWindows ? cases.WIN : cases.POSIX;
platformCases.forEach((c) => {
  const paths = _module._nodeModulePaths(c.file);
  assert.deepStrictEqual(
    c.expect, paths,
    `case ${c.file} failed, actual paths is ${JSON.stringify(paths)}`);
});
