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

var HUGE = 64 * 1024;

var originalBuffer = new Buffer(3 * HUGE);
var originalOffset = 0;

var gzipBuffer = new Buffer(3 * HUGE);
var gzipOffset = 0;

function getRandomLetter() {
  return (Math.random() * (122 - 97)) + 97;
}

function generateHugeStream() {
  var buffer = new Buffer(HUGE);
  for (var i = 0; i < HUGE; i++)
    buffer.writeUInt8(getRandomLetter(), i);

  buffer.copy(originalBuffer, originalOffset);
  originalOffset += HUGE;

  return buffer;
}

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

writeGzipStream(generateHugeStream(), function() {
  writeGzipStream(generateHugeStream(), function() {
    writeGzipStream(generateHugeStream(), function() {
      var gunzip = zlib.createGunzip();
      var gunzippedData = new Buffer(3 * HUGE);
      var gunzippedOffset = 0;
      gunzip.on('data', function (data) {
        data.copy(gunzippedData, gunzippedOffset);
        gunzippedOffset += data.length;
      });
      gunzip.on('end', function() {
        var gunzippedStr = gunzippedData.toString('utf8', 0, gunzippedOffset);
        var originalStr  = originalBuffer.toString('utf8', 0, 3 * HUGE);

        assert.equal(gunzippedStr, originalStr);
      });

      gunzip.write(gzipBuffer.slice(0, gzipOffset), 'binary', function() {
        gunzip.end();
      });
    });
  });
});
