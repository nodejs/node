'use strict';
var assert = require('assert');
var util = require('util');
var join = require('path').join;
var fs = require('fs');
var common = require('../common');

common.refreshTmpDir();

var repl = require('repl');

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    var self = this;
    data.forEach(function(line) {
      self.emit('data', line + '\n');
    });
  };
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
  'var top = function() {',
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

// should not break
putIn.write = function(data) {
  // make sure I get a failed to load message and not some crazy error
  assert.equal(data, 'Failed to load:' + loadFile + '\n');
  // eat me to avoid work
  putIn.write = function() {};
};
putIn.run(['.load ' + loadFile]);

// clear the REPL
putIn.run(['.clear']);

// NUL (\0) is disallowed in filenames in UNIX-like operating systems and
// Windows so we can use that to test failed saves
const invalidFileName = join(common.tmpDir, '\0\0\0\0\0');

// should not break
putIn.write = function(data) {
  // make sure I get a failed to save message and not some other error
  assert.equal(data, 'Failed to save:' + invalidFileName + '\n');
  // reset to no-op
  putIn.write = function() {};
};

// save it to a file
putIn.run(['.save ' + invalidFileName]);
