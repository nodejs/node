var common = module.exports;

var path = require('path');
var fs = require('fs');
var root = path.join(__dirname, '..');

common.dir = {
  fixture: root + '/test/fixture',
  tmp: root + '/test/tmp',
};

// Create tmp directory if it does not exist
// Not using fs.exists so as to be node 0.6.x compatible
try {
  fs.statSync(common.dir.tmp);
}
catch (e) {
  // Dir does not exist
  fs.mkdirSync(common.dir.tmp);
}

common.CombinedStream = require(root);
common.assert = require('assert');
