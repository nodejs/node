'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const fn = path.join(common.fixturesDir, 'non-existent');
const existingFile = path.join(common.fixturesDir, 'exit.js');
const existingFile2 = path.join(common.fixturesDir, 'create-file.js');
const existingDir = path.join(common.fixturesDir, 'empty');
const existingDir2 = path.join(common.fixturesDir, 'keys');

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
  assert.ok(0 <= err.message.indexOf(existingFile));
  assert.ok(0 <= err.message.indexOf(existingFile2));
});

fs.symlink(existingFile, existingFile2, function(err) {
  assert.ok(0 <= err.message.indexOf(existingFile));
  assert.ok(0 <= err.message.indexOf(existingFile2));
});

fs.unlink(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.rename(fn, 'foo', function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.rename(existingDir, existingDir2, function(err) {
  assert.ok(0 <= err.message.indexOf(existingDir));
  assert.ok(0 <= err.message.indexOf(existingDir2));
});

fs.rmdir(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.mkdir(existingFile, 0o666, function(err) {
  assert.ok(0 <= err.message.indexOf(existingFile));
});

fs.rmdir(existingFile, function(err) {
  assert.ok(0 <= err.message.indexOf(existingFile));
});

fs.chmod(fn, 0o666, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.open(fn, 'r', 0o666, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.readFile(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

// Sync

const errors = [];
let expected = 0;

try {
  ++expected;
  fs.statSync(fn);
} catch (err) {
  errors.push('stat');
  assert.ok(0 <= err.message.indexOf(fn));
}

try {
  ++expected;
  fs.mkdirSync(existingFile, 0o666);
} catch (err) {
  errors.push('mkdir');
  assert.ok(0 <= err.message.indexOf(existingFile));
}

try {
  ++expected;
  fs.chmodSync(fn, 0o666);
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
  assert.ok(0 <= err.message.indexOf(existingFile));
  assert.ok(0 <= err.message.indexOf(existingFile2));
}

try {
  ++expected;
  fs.symlinkSync(existingFile, existingFile2);
} catch (err) {
  errors.push('symlink');
  assert.ok(0 <= err.message.indexOf(existingFile));
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
  assert.ok(0 <= err.message.indexOf(existingDir));
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
