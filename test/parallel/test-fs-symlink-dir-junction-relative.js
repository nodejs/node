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

// Test creating and resolving relative junction or symbolic link

var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var completed = 0;
var expected_tests = 2;

var linkPath1 = path.join(common.tmpDir, 'junction1');
var linkPath2 = path.join(common.tmpDir, 'junction2');
var linkTarget = path.join(common.fixturesDir);
var linkData = '../fixtures';

// Prepare.
try { fs.mkdirSync(common.tmpDir); } catch (e) {}
try { fs.unlinkSync(linkPath1); } catch (e) {}
try { fs.unlinkSync(linkPath2); } catch (e) {}

// Test fs.symlink()
fs.symlink(linkData, linkPath1, 'junction', function(err) {
  if (err) throw err;
  verifyLink(linkPath1);
});

// Test fs.symlinkSync()
fs.symlinkSync(linkData, linkPath2, 'junction');
verifyLink(linkPath2);

function verifyLink(linkPath) {
  var stats = fs.lstatSync(linkPath);
  assert.ok(stats.isSymbolicLink());

  var data1 = fs.readFileSync(linkPath + '/x.txt', 'ascii');
  var data2 = fs.readFileSync(linkTarget + '/x.txt', 'ascii');
  assert.strictEqual(data1, data2);

  // Clean up.
  fs.unlinkSync(linkPath);

  completed++;
}

process.on('exit', function() {
  assert.equal(completed, expected_tests);
});

