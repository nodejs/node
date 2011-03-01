var common = require('../common');
var assert = require('assert');

var path = require('path'),
    fs = require('fs');

var writeEndOk = false;
(function() {
  debugger;
  var file = path.join(common.tmpDir, 'write-end-test.txt');
  var stream = fs.createWriteStream(file);

  stream.end('a\n', 'utf8', function() {
    var content = fs.readFileSync(file, 'utf8');
    assert.equal(content, 'a\n');
    writeEndOk = true;
  });

})();


process.on('exit', function() {
  assert.ok(writeEndOk);
});

