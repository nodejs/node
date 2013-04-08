var common = require('../common');
var assert = common.assert;
var CombinedStream = common.CombinedStream;
var fs = require('fs');

var FILE1 = common.dir.fixture + '/file1.txt';
var BUFFER = new Buffer('Bacon is delicious');
var FILE2 = common.dir.fixture + '/file2.txt';
var STRING = 'The € kicks the $\'s ass!';

var EXPECTED =
  fs.readFileSync(FILE1)
  + BUFFER
  + fs.readFileSync(FILE2)
  + STRING;
var GOT;

(function testDelayedStreams() {
  var combinedStream = CombinedStream.create();
  combinedStream.append(fs.createReadStream(FILE1));
  combinedStream.append(BUFFER);
  combinedStream.append(fs.createReadStream(FILE2));
  combinedStream.append(function(next) {
    next(STRING);
  });

  var tmpFile = common.dir.tmp + '/combined-file1-buffer-file2-string.txt';
  var dest = fs.createWriteStream(tmpFile);
  combinedStream.pipe(dest);

  dest.on('close', function() {
    GOT = fs.readFileSync(tmpFile, 'utf8');
  });
})();

process.on('exit', function() {
  assert.strictEqual(GOT, EXPECTED);
});
