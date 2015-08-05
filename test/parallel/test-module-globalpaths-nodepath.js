'use strict';
const common = require('../common');
const assert = require('assert');

const module_ = require('module');

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

module_._initPaths();

assert.ok(module_.globalPaths.indexOf(partA) !== -1);
assert.ok(module_.globalPaths.indexOf(partB) !== -1);
assert.ok(module_.globalPaths.indexOf(partC) === -1);

assert.ok(Array.isArray(module_.globalPaths));
