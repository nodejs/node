var common = require('../common');
var assert = require('assert');
var path = require('path');

var isDebug = (process.version.indexOf('debug') >= 0);

var debugPath = path.normalize(path.join(__dirname, '..', '..',
                                         'build', 'debug', 'node_g'));
var defaultPath = path.normalize(path.join(__dirname, '..', '..',
                                           'build', 'default', 'node'));

console.log('debugPath: ' + debugPath);
console.log('defaultPath: ' + defaultPath);
console.log('process.execPath: ' + process.execPath);

if (/node_g$/.test(process.execPath)) {
  assert.equal(debugPath, process.execPath);
} else {
  assert.equal(defaultPath, process.execPath);
}

