var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var path = require('path');
var fs = require('fs');
var exits = 0;

// Test `env` parameter
// for child_process.spawn(path, args, env, customFds) deprecated api
(function() {
  var response = '';
  var child = spawn('/usr/bin/env', [], {'HELLO': 'WORLD'});

  child.stdout.setEncoding('utf8');

  child.stdout.addListener('data', function(chunk) {
    response += chunk;
  });

  process.addListener('exit', function() {
    assert.ok(response.indexOf('HELLO=WORLD') >= 0);
    exits++;
  });
})();

// Test `customFds` parameter
// for child_process.spawn(path, args, env, customFds) deprecated api
(function() {
  var expected = 'hello world';
  var helloPath = path.join(common.fixturesDir, 'hello.txt');

  fs.open(helloPath, 'w', 400, function(err, fd) {
    if (err) throw err;

    var child = spawn('/bin/echo', [expected], undefined, [-1, fd]);

    assert.notEqual(child.stdin, null);
    assert.equal(child.stdout, null);
    assert.notEqual(child.stderr, null);

    child.addListener('exit', function(err) {
      if (err) throw err;

      fs.close(fd, function(err) {
        if (err) throw err;

        fs.readFile(helloPath, function(err, data) {
          if (err) throw err;

          assert.equal(data.toString(), expected + '\n');
          exits++;
        });
      });
    });
  });
})();

// Check if all child processes exited
process.addListener('exit', function() {
  assert.equal(2, exits);
});
