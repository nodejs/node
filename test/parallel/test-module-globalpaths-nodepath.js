'use strict';
const common = require('../common');
const assert = require('assert');

const mod = require('module');

let partA, partB;
const partC = '';

if (common.isWindows) {
  partA = 'C:\\Users\\Rocko Artischocko\\AppData\\Roaming\\npm';
  partB = 'C:\\Program Files (x86)\\nodejs\\';
  process.env['NODE_PATH'] = partA + ';' + partB + ';' + partC;
} else {
  partA = '/usr/test/lib/node_modules';
  partB = '/usr/test/lib/node';
  process.env['NODE_PATH'] = partA + ':' + partB + ':' + partC;
}

mod._initPaths();

assert.ok(mod.globalPaths.indexOf(partA) !== -1);
assert.ok(mod.globalPaths.indexOf(partB) !== -1);
assert.ok(mod.globalPaths.indexOf(partC) === -1);

assert.ok(Array.isArray(mod.globalPaths));
