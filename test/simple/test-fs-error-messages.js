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




var common = require('../common');
var assert = require('assert');

var path = require('path'),
    fs = require('fs'),
    fn = path.join(common.fixturesDir, 'non-existent'),
    existingFile = path.join(common.fixturesDir, 'exit.js'),
    existingFile2 = path.join(common.fixturesDir, 'create-file.js'),
    existingDir = path.join(common.fixturesDir, 'empty'),
    existingDir2 = path.join(common.fixturesDir, 'keys');

// ASYNC_CALL

fs.stat(fn, function(err) {
  assert.equal(fn, err.path);
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.lstat(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.readlink(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.link(fn, 'foo', function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.link(existingFile, existingFile2, function(err) {
  assert.ok(0 <= err.message.indexOf(existingFile2));
});

fs.unlink(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.rename(fn, 'foo', function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.rename(existingDir, existingDir2, function(err) {
  assert.ok(0 <= err.message.indexOf(existingDir2));
});

fs.rmdir(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.mkdir(existingFile, 0666, function(err) {
  assert.ok(0 <= err.message.indexOf(existingFile));
});

fs.rmdir(existingFile, function(err) {
  assert.ok(0 <= err.message.indexOf(existingFile));
});

fs.chmod(fn, 0666, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.open(fn, 'r', 0666, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.readFile(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

// Sync

var errors = [],
    expected = 0;

try {
  ++expected;
  fs.statSync(fn);
} catch (err) {
  errors.push('stat');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.mkdirSync(existingFile, 0666);
} catch (err) {
  errors.push('mkdir');
  assert.ok(0 <= err.message.indexOf(existingFile));
}

try {
  ++expected;
  fs.chmodSync(fn, 0666);
} catch (err) {
  errors.push('chmod');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.lstatSync(fn);
} catch (err) {
  errors.push('lstat');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.readlinkSync(fn);
} catch (err) {
  errors.push('readlink');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.linkSync(fn, 'foo');
} catch (err) {
  errors.push('link');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.linkSync(existingFile, existingFile2);
} catch (err) {
  errors.push('link');
  assert.ok(0 <= err.message.indexOf(existingFile2));
}

try {
  ++expected;
  fs.unlinkSync(fn);
} catch (err) {
  errors.push('unlink');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.rmdirSync(fn);
} catch (err) {
  errors.push('rmdir');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.rmdirSync(existingFile);
} catch (err) {
  errors.push('rmdir');
  assert.ok(0 <= err.message.indexOf(existingFile));
}

try {
  ++expected;
  fs.openSync(fn, 'r');
} catch (err) {
  errors.push('opens');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.renameSync(fn, 'foo');
} catch (err) {
  errors.push('rename');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.renameSync(existingDir, existingDir2);
} catch (err) {
  errors.push('rename');
  assert.ok(0 <= err.message.indexOf(existingDir2));
}

try {
  ++expected;
  fs.readdirSync(fn);
} catch (err) {
  errors.push('readdir');
  assert.ok(0 <= err.message.indexOf(fn));
}

process.on('exit', function() {
  assert.equal(expected, errors.length,
               'Test fs sync exceptions raised, got ' + errors.length +
               ' expected ' + expected);
});
