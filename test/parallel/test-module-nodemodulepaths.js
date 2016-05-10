'use strict';

const common = require('../common');
const assert = require('assert');
const _module = require('module');

const cases = {
  'WIN': [{
    file: 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo',
    expect: [
      'C:\\Users\\Rocko Artischocko\\node_stuff\\foo\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_stuff\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_modules',
      'C:\\Users\\node_modules',
      'C:\\node_modules',
    ]
  }, {
    file: 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo_node_modules',
    expect: [
      'C:\\Users\\Rocko \
Artischocko\\node_stuff\\foo_node_modules\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_stuff\\node_modules',
      'C:\\Users\\Rocko Artischocko\\node_modules',
      'C:\\Users\\node_modules',
      'C:\\node_modules',
    ]
  }],
  'UNIX': [{
    file: '/usr/test/lib/node_modules/npm/foo',
    expect: [
      '/usr/test/lib/node_modules/npm/foo/node_modules',
      '/usr/test/lib/node_modules/npm/node_modules',
      '/usr/test/lib/node_modules',
      '/usr/test/node_modules',
      '/usr/node_modules',
    ]
  }, {
    file: '/usr/test/lib/node_modules/npm/foo_node_modules',
    expect: [
      '/usr/test/lib/node_modules/npm/foo_node_modules/node_modules',
      '/usr/test/lib/node_modules/npm/node_modules',
      '/usr/test/lib/node_modules',
      '/usr/test/node_modules',
      '/usr/node_modules',
    ]
  }]
};

const platformCases = common.isWindows ? cases.WIN : cases.UNIX;
platformCases.forEach((c) => {
  const paths = _module._nodeModulePaths(c.file);
  assert.deepStrictEqual(c.expect, paths);
});
