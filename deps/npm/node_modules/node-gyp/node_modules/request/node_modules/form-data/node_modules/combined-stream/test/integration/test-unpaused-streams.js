var common = require('../common');
var assert = common.assert;
var CombinedStream = common.CombinedStream;
var fs = require('fs');

var FILE1 = common.dir.fixture + '/file1.txt';
var FILE2 = common.dir.fixture + '/file2.txt';
var EXPECTED = fs.readFileSync(FILE1) + fs.readFileSync(FILE2);

(function testDelayedStreams() {
  var combinedStream = CombinedStream.create({pauseStreams: false});
  combinedStream.append(fs.createReadStream(FILE1));
  combinedStream.append(fs.createReadStream(FILE2));

  var stream1 = combinedStream._streams[0];
  var stream2 = combinedStream._streams[1];

  stream1.on('end', function() {
    assert.ok(stream2.dataSize > 0);
  });

  var tmpFile = common.dir.tmp + '/combined.txt';
  var dest = fs.createWriteStream(tmpFile);
  combinedStream.pipe(dest);

  dest.on('end', function() {
    var written = fs.readFileSync(tmpFile, 'utf8');
    assert.strictEqual(written, EXPECTED);
  });
})();
