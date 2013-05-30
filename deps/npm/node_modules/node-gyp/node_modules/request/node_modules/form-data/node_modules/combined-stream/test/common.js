var common = module.exports;

var path = require('path');
var root = path.join(__dirname, '..');

common.dir = {
  fixture: root + '/test/fixture',
  tmp: root + '/test/tmp',
};

common.CombinedStream = require(root);
common.assert = require('assert');
