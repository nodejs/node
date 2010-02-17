process.mixin(require("./common"));

var
  count = 100,
  fs = require('fs');

function tryToKillEventLoop() {
  puts('trying to kill event loop ...');

  fs.stat(__filename)
    .addCallback(function() {
      puts('first fs.stat succeeded ...');

      fs.stat(__filename)
        .addCallback(function() {
          puts('second fs.stat succeeded ...');
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
          throw new Exception('second fs.stat failed')
        })

    })
    .addErrback(function() {
      throw new Exception('first fs.stat failed')
    });
}

// Generate a lot of thread pool events
var pos = 0;
fs.open('/dev/zero', "r").addCallback(function (rd) {
  function readChunk () {
    fs.read(rd, 1024, pos, 'binary').addCallback(function (chunk, bytesRead) {
      if (chunk) {
        pos += bytesRead;
        //puts(pos);
        readChunk();
      } else {
        fs.close(rd);
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
