var fs = require('fs');
var common = require('../common');
var assert = common.assert;
var CombinedStream = common.CombinedStream;
var FILE1 = common.dir.fixture + '/file1.txt';
var fileStream = fs.createReadStream(FILE1);

var foo = function(){};

(function testIsStreamLike() {
  assert(! CombinedStream.isStreamLike(true));
  assert(! CombinedStream.isStreamLike("I am a string"));
  assert(! CombinedStream.isStreamLike(7));
  assert(! CombinedStream.isStreamLike(foo));

  assert(CombinedStream.isStreamLike(fileStream));
})();