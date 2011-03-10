// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// We are demonstrating a problem with http.get when queueing up many
// transfers. The server simply introduces some delay and sends a file.
// Note this is demonstarted with connection: close.
var common = require('../common');
var assert = require('assert');
var http = require('http');
var fs = require('fs');

var image = fs.readFileSync(common.fixturesDir + '/person.jpg');

console.log("image.length = " + image.length);

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
        console.error("recv " + x);
        var s = fs.createWriteStream(common.tmpDir + '/' + x + ".jpg");
        res.pipe(s);

        // TODO there should be a callback to pipe() that will allow
        // us to get a callback when the pipe is finished.
        res.on('end', function() {
          console.error("done " + x);
          if (++responses == total) {
            s.on('close', checkFiles);
          }
        });
      }).on('error', function(e) {
        console.error('error! ', e.message)
        throw e;
      });
    })();
  }
});


var checkedFiles = false;
function checkFiles() {
  // Should see 1.jpg, 2.jpg, ..., 100.jpg in tmpDir
  var files = fs.readdirSync(common.tmpDir);
  assert.equal(total, files.length);

  for (var i = 0; i < total; i++) {
    var fn = i + '.jpg';
    assert.ok(files.indexOf(fn) >= 0, "couldn't find '" + fn + "'");
    var stat = fs.statSync(common.tmpDir + '/' + fn);
    assert.equal(image.length, stat.size,
                 "size doesn't match on '" + fn +
                 "'. Got " + stat.size + " bytes");
  }

  checkedFiles = true;
}


process.on('exit', function() {
  assert.equal(total, requests);
  assert.equal(total, responses);
  assert.ok(checkedFiles);
});
