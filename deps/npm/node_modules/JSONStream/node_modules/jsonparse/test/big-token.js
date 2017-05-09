var stream = require('stream');
var JsonParse = require('../jsonparse');
var test = require('tape');

test('can handle large tokens without running out of memory', function (t) {
  var parser = new JsonParse();
  var chunkSize = 1024;
  var chunks = 1024 * 200; // 200mb
  var quote = Buffer.from ? Buffer.from('"') : new Buffer('"');
  t.plan(1);

  parser.onToken = function (type, value) {
    t.equal(value.length, chunkSize * chunks, 'token should be size of input json');
    t.end();
  };

  parser.write(quote);
  for (var i = 0; i < chunks; ++i) {
    var buf = Buffer.alloc ? Buffer.alloc(chunkSize) : new Buffer(chunkSize);
    buf.fill('a');
    parser.write(buf);
  }
  parser.write(quote);
});
