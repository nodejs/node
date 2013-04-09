var common = require('../common');
var assert = common.assert;
var CombinedStream = common.CombinedStream;
var fs = require('fs');

var FILE1 = common.dir.fixture + '/file1.txt';
var FILE2 = common.dir.fixture + '/file2.txt';
var EXPECTED = fs.readFileSync(FILE1) + fs.readFileSync(FILE2);

(function testDelayedStreams() {
  var combinedStream = CombinedStream.create({pauseStreams: false, maxDataSize: 20736});
  combinedStream.append(fs.createReadStream(FILE1));
  combinedStream.append(fs.createReadStream(FILE2));

  var gotErr = null;
  combinedStream.on('error', function(err) {
    gotErr = err;
  });

  process.on('exit', function() {
    assert.ok(gotErr);
    assert.ok(gotErr.message.match(/bytes/));
  });
})();
