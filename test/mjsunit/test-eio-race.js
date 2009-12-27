process.mixin(require("./common"));

var
  count = 100,
  posix = require('posix');

function tryToKillEventLoop() {
  puts('trying to kill event loop ...');

  posix.stat(__filename)
    .addCallback(function() {
      puts('first posix.stat succeeded ...');

      posix.stat(__filename)
        .addCallback(function() {
          puts('second posix.stat succeeded ...');
          puts('could not kill event loop, retrying...');

          setTimeout(function () {
            if (--count) {
              tryToKillEventLoop();
            } else {
              process.exit(0);
            }
          }, 1);
        })
        .addErrback(function() {
          throw new Exception('second posix.stat failed')
        })

    })
    .addErrback(function() {
      throw new Exception('first posix.stat failed')
    });
}

// Generate a lot of thread pool events
var pos = 0;
posix.open('/dev/zero', process.O_RDONLY, 0666).addCallback(function (rd) {
  function readChunk () {
    posix.read(rd, 1024, pos, 'binary').addCallback(function (chunk, bytesRead) {
      if (chunk) {
        pos += bytesRead;
        //puts(pos);
        readChunk();
      } else {
        posix.close(rd);
        throw new Exception(BIG_FILE+' should not end before the issue shows up');
      }
    }).addErrback(function () {
      throw new Exception('could not read from '+BIG_FILE);
    });
  }
  readChunk();
}).addErrback(function () {
  throw new Exception('could not open '+BIG_FILE);
});

tryToKillEventLoop();

process.addListener("exit", function () {
  assert.ok(pos > 10000);
});
