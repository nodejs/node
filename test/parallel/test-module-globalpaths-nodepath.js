'use strict';
var common = require('../common');
var assert = require('assert');

var module = require('module');

var partA, partB;
var partC = '';

if (common.isWindows) {
  partA = 'C:\\Users\\Rocko Artischocko\\AppData\\Roaming\\npm';
  partB = 'C:\\Program Files (x86)\\nodejs\\';
  process.env['NODE_PATH'] = partA + ';' + partB + ';' + partC;
} else {
  partA = '/usr/test/lib/node_modules';
  partB = '/usr/test/lib/node';
  process.env['NODE_PATH'] = partA + ':' + partB + ':' + partC;
}

module._initPaths();

assert.ok(module.globalPaths.indexOf(partA) !== -1);
assert.ok(module.globalPaths.indexOf(partB) !== -1);
assert.ok(module.globalPaths.indexOf(partC) === -1);

assert.ok(Array.isArray(module.globalPaths));
