var common = require('../common');
var assert = require('assert');

var path = require('path'),
    fs = require('fs'),
    fn = path.join(common.fixturesDir, 'non-existent'),
    existingFile = path.join(common.fixturesDir, 'exit.js');

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

fs.unlink(fn, function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
});

fs.rename(fn, 'foo', function(err) {
  assert.ok(0 <= err.message.indexOf(fn));
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
  fs.readdirSync(fn);
} catch (err) {
  errors.push('readdir');
  assert.ok(0 <= err.message.indexOf(fn));
}

process.addListener('exit', function() {
  assert.equal(expected, errors.length,
               'Test fs sync exceptions raised, got ' + errors.length +
               ' expected ' + expected);
});
