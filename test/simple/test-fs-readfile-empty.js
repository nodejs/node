require('../common');

var
  path = require('path'),
  fs = require('fs'),
  fn = path.join(fixturesDir, 'empty.txt');

fs.readFile(fn, function(err, data) {
  assert.ok(data);
});