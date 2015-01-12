var spawn = require('child_process').spawn,
  assert = require('assert'),
  windows = (process.platform === 'win32'),
  cmd = (windows) ? 'rundll32' : 'ls',
  invalidcmd = 'hopefully_you_dont_have_this_on_your_machine',
  invalidArgsMsg = /Incorrect value of args option/,
  invalidOptionsMsg = /options argument must be an object/,
  errors = 0;

try {
  // Ensure this throws a TypeError
  var child = spawn(invalidcmd, 'this is not an array');

  child.on('error', function (err) {
    errors++;
  });

} catch (e) {
  assert.equal(e instanceof TypeError, true);
}

// verify that valid argument combinations do not throw
assert.doesNotThrow(function() {
  spawn(cmd);
});

assert.doesNotThrow(function() {
  spawn(cmd, []);
});

assert.doesNotThrow(function() {
  spawn(cmd, {});
});

assert.doesNotThrow(function() {
  spawn(cmd, [], {});
});

// verify that invalid argument combinations throw
assert.throws(function() {
  spawn();
}, /Bad argument/);

assert.throws(function() {
  spawn(cmd, null);
}, invalidArgsMsg);

assert.throws(function() {
  spawn(cmd, true);
}, invalidArgsMsg);

assert.throws(function() {
  spawn(cmd, [], null);
}, invalidOptionsMsg);

assert.throws(function() {
  spawn(cmd, [], 1);
}, invalidOptionsMsg);

process.on('exit', function() {
  assert.equal(errors, 0);
});
