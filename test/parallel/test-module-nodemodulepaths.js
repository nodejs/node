'use strict';
const common = require('../common');
const assert = require('assert');

module = require('module');

let file, delimiter;

if (common.isWindows) {
  file = 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo';
  delimiter = '\\';
} else {
  file = '/usr/test/lib/node_modules/npm/foo';
  delimiter = '/';
}

const paths = module._nodeModulePaths(file);

assert.ok(paths.indexOf(file + delimiter + 'node_modules') !== -1);
assert.ok(Array.isArray(paths));
