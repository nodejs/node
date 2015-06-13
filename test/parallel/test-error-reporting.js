'use strict';
var common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;
var path = require('path');

var exits = 0;

function errExec(script, callback) {
  var cmd = '"' + process.argv[0] + '" "' +
      path.join(common.fixturesDir, script) + '"';
  return exec(cmd, function(err, stdout, stderr) {
    // There was some error
    assert.ok(err);

    // More than one line of error output.
    assert.ok(stderr.split('\n').length > 2);

    // Assert the script is mentioned in error output.
    assert.ok(stderr.indexOf(script) >= 0);

    // Proxy the args for more tests.
    callback(err, stdout, stderr);

    // Count the tests
    exits++;

    console.log('.');
  });
}


// Simple throw error
errExec('throws_error.js', function(err, stdout, stderr) {
  assert.ok(/blah/.test(stderr));
});


// Trying to JSON.parse(undefined)
errExec('throws_error2.js', function(err, stdout, stderr) {
  assert.ok(/SyntaxError/.test(stderr));
});


// Trying to JSON.parse(undefined) in nextTick
errExec('throws_error3.js', function(err, stdout, stderr) {
  assert.ok(/SyntaxError/.test(stderr));
});


// throw ILLEGAL error
errExec('throws_error4.js', function(err, stdout, stderr) {
  assert.ok(/\/\*\*/.test(stderr));
  assert.ok(/SyntaxError/.test(stderr));
});


process.on('exit', function() {
  assert.equal(4, exits);
});
