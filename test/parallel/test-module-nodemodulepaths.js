'use strict';
var common = require('../common');
var assert = require('assert');

var module = require('module');

var isWindows = process.platform === 'win32';

var file, delimiter, paths;

if (isWindows) {
  file = 'C:\\Users\\Rocko Artischocko\\node_stuff\\foo';
  delimiter = '\\';
} else {
  file = '/usr/test/lib/node_modules/npm/foo';
  delimiter = '/';
}

paths = module._nodeModulePaths(file);

assert.ok(paths.indexOf(file + delimiter + 'node_modules') !== -1);
assert.ok(Array.isArray(paths));