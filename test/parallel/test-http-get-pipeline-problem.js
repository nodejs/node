'use strict';
// In previous versions of Node.js (e.g., 0.6.0), this sort of thing would halt
// after http.globalAgent.maxSockets number of files.
// See https://groups.google.com/forum/#!topic/nodejs-dev/V5fB69hFa9o
var common = require('../common');
var assert = require('assert');
var http = require('http');
var fs = require('fs');

http.globalAgent.maxSockets = 1;

common.refreshTmpDir();

var image = fs.readFileSync(common.fixturesDir + '/person.jpg');

console.log('image.length = ' + image.length);

var total = 10;
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


server.listen(0, function() {
  for (var i = 0; i < total; i++) {
    (function() {
      var x = i;

      var opts = {
        port: server.address().port,
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
