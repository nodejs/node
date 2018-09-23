'use strict';
// We are demonstrating a problem with http.get when queueing up many
// transfers. The server simply introduces some delay and sends a file.
// Note this is demonstrated with connection: close.
var common = require('../common');
var assert = require('assert');
var http = require('http');
var fs = require('fs');

common.refreshTmpDir();

var image = fs.readFileSync(common.fixturesDir + '/person.jpg');

console.log('image.length = ' + image.length);

var total = 100;
var requests = 0, responses = 0;

var server = http.Server(function(req, res) {
  if (++requests == total) {
    server.close();
  }

  setTimeout(function() {
    res.writeHead(200, {
      'content-type': 'image/jpeg',
      'connection': 'close',
      'content-length': image.length
    });
    res.end(image);
  }, 1);
});


server.listen(common.PORT, function() {
  for (var i = 0; i < total; i++) {
    (function() {
      var x = i;

      var opts = {
        port: common.PORT,
        headers: { connection: 'close' }
      };

      http.get(opts, function(res) {
        console.error('recv ' + x);
        var s = fs.createWriteStream(common.tmpDir + '/' + x + '.jpg');
        res.pipe(s);

        s.on('finish', function() {
          console.error('done ' + x);
          if (++responses == total) {
            checkFiles();
          }
        });
      }).on('error', function(e) {
        console.error('error! ', e.message);
        throw e;
      });
    })();
  }
});


var checkedFiles = false;
function checkFiles() {
  // Should see 1.jpg, 2.jpg, ..., 100.jpg in tmpDir
  var files = fs.readdirSync(common.tmpDir);
  assert(total <= files.length);

  for (var i = 0; i < total; i++) {
    var fn = i + '.jpg';
    assert.ok(files.indexOf(fn) >= 0, "couldn't find '" + fn + "'");
    var stat = fs.statSync(common.tmpDir + '/' + fn);
    assert.equal(image.length, stat.size,
                 "size doesn't match on '" + fn +
                 "'. Got " + stat.size + ' bytes');
  }

  checkedFiles = true;
}


process.on('exit', function() {
  assert.equal(total, requests);
  assert.equal(total, responses);
  assert.ok(checkedFiles);
});
