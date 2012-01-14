var assert = require('assert'),
    zlib = require('zlib'),
    started = 0,
    done = 0;

function repeat(fn) {
  if (started != 0) {
    assert.ok(started - done < 100)
  }

  process.nextTick(function() {
    fn();
    repeat(fn);
  });
}

repeat(function() {
  if (started > 1000) return process.exit(0);

  for (var i = 0; i < 30; i++) {
    started++;
    var deflate = zlib.createDeflate();
    deflate.write('123');
    deflate.flush(function() {
      done++;
    });
  }
});
