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

var assert = require('assert');
var util = require('util');
var join = require('path').join;
var fs = require('fs');
var common = require('../common');

var repl = require('repl');

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    var self = this;
    data.forEach(function(line) {
      self.emit('data', line);
    });
  }
}
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

var works = [['inner.one'], 'inner.o'];

var putIn = new ArrayStream();
var testMe = repl.start('', putIn);


var testFile = [
  'var top = function () {',
  'var inner = {one:1};'
];
var saveFileName = join(common.tmpDir, 'test.save.js');

// input some data
putIn.run(testFile);

// save it to a file
putIn.run(['.save ' + saveFileName]);

// the file should have what I wrote
assert.equal(fs.readFileSync(saveFileName, 'utf8'), testFile.join('\n') + '\n');

// make sure that the REPL data is "correct"
// so when I load it back I know I'm good
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

// clear the REPL
putIn.run(['.clear']);

// Load the file back in
putIn.run(['.load ' + saveFileName]);

// make sure that the REPL data is "correct"
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

// clear the REPL
putIn.run(['.clear']);

var loadFile = join(common.tmpDir, 'file.does.not.exist');

// shold not break
putIn.write = function(data) {
  // make sure I get a failed to load message and not some crazy error
  assert.equal(data, 'Failed to load:' + loadFile + '\n');
  // eat me to avoid work
  putIn.write = function() {};
};
putIn.run(['.load ' + loadFile]);


//TODO how do I do a failed .save test?
