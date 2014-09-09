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

// test unzipping a file that was created by concatenating multiple gzip
// streams.

var common = require('../common');
var assert = require('assert');
var zlib = require('zlib');

var util = require('util');

var gzipBuffer = new Buffer(128);
var gzipOffset = 0;

var stream1 = '123\n';
var stream2 = '456\n';
var stream3 = '789\n';

function gzipAppend(data) {
  data.copy(gzipBuffer, gzipOffset);
  gzipOffset += data.length;
}

function writeGzipStream(text, cb) {
  var gzip = zlib.createGzip();
  gzip.on('data', gzipAppend);
  gzip.write(text, function() {
    gzip.flush(function() {
      gzip.end(function() {
        cb();
      });
    });
  });
}

writeGzipStream(stream1, function() {
  writeGzipStream(stream2, function() {
    writeGzipStream(stream3, function() {
      var gunzip = zlib.createGunzip();
      var gunzippedData = new Buffer(2 * 1024);
      var gunzippedOffset = 0;
      gunzip.on('data', function (data) {
        data.copy(gunzippedData, gunzippedOffset);
        gunzippedOffset += data.length;
      });
      gunzip.on('end', function() {
        assert.equal(gunzippedData.toString('utf8', 0, gunzippedOffset), stream1 + stream2 + stream3);
      });

      gunzip.write(gzipBuffer.slice(0, gzipOffset), 'binary', function() {
        gunzip.end();
      });
    });
  });
});
