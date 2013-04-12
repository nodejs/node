var common = require('../common');
var assert = common.assert;
var CombinedStream = common.CombinedStream;
var fs = require('fs');

var FILE1 = common.dir.fixture + '/file1.txt';
var FILE2 = common.dir.fixture + '/file2.txt';
var EXPECTED = fs.readFileSync(FILE1) + fs.readFileSync(FILE2);

(function testDelayedStreams() {
  var combinedStream = CombinedStream.create();
  combinedStream.append(function(next) {
    next(fs.createReadStream(FILE1));
  });
  combinedStream.append(function(next) {
    next(fs.createReadStream(FILE2));
  });

  var tmpFile = common.dir.tmp + '/combined.txt';
  var dest = fs.createWriteStream(tmpFile);
  combinedStream.pipe(dest);

  dest.on('end', function() {
    var written = fs.readFileSync(tmpFile, 'utf8');
    assert.strictEqual(written, EXPECTED);
  });
})();
