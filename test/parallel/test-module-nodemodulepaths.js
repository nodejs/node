'use strict';
const common = require('../common');
const assert = require('assert');

const module_ = require('module');

var file, delimiter, paths;

if (common.isWindows) {
  file = 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo';
  delimiter = '\\';
} else {
  file = '/usr/test/lib/node_modules/npm/foo';
  delimiter = '/';
}

paths = module_._nodeModulePaths(file);

assert.ok(paths.indexOf(file + delimiter + 'node_modules') !== -1);
assert.ok(Array.isArray(paths));
