common = require("../common");
assert = common.assert

var
  path = require('path'),
  fs = require('fs'),
  fn = path.join(common.fixturesDir, 'empty.txt');

fs.readFile(fn, function(err, data) {
  assert.ok(data);
});

fs.readFile(fn, 'utf8', function(err, data) {
  assert.strictEqual('', data);
});