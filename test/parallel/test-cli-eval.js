'use strict';
if (module.parent) {
  // signal we've been loaded as a module
  console.log('Loaded as a module, exiting with status code 42.');
  process.exit(42);
}

var common = require('../common'),
    assert = require('assert'),
    child = require('child_process'),
    nodejs = '"' + process.execPath + '"';


// replace \ by / because windows uses backslashes in paths, but they're still
// interpreted as the escape character when put between quotes.
var filename = __filename.replace(/\\/g, '/');

// assert that nothing is written to stdout
child.exec(nodejs + ' --eval 42',
    function(err, stdout, stderr) {
      assert.equal(stdout, '');
      assert.equal(stderr, '');
    });

// assert that "42\n" is written to stderr
child.exec(nodejs + ' --eval "console.error(42)"',
    function(err, stdout, stderr) {
      assert.equal(stdout, '');
      assert.equal(stderr, '42\n');
    });

// assert that the expected output is written to stdout
['--print', '-p -e', '-pe', '-p'].forEach(function(s) {
  var cmd = nodejs + ' ' + s + ' ';

  child.exec(cmd + '42',
      function(err, stdout, stderr) {
        assert.equal(stdout, '42\n');
        assert.equal(stderr, '');
      });

  child.exec(cmd + "'[]'",
      function(err, stdout, stderr) {
        assert.equal(stdout, '[]\n');
        assert.equal(stderr, '');
      });
});

// assert that module loading works
child.exec(nodejs + ' --eval "require(\'' + filename + '\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });

// module path resolve bug, regression test
child.exec(nodejs + ' --eval "require(\'./test/parallel/test-cli-eval.js\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });

// empty program should do nothing
child.exec(nodejs + ' -e ""', function(status, stdout, stderr) {
  assert.equal(stdout, '');
  assert.equal(stderr, '');
});

// "\\-42" should be interpreted as an escaped expression, not a switch
child.exec(nodejs + ' -p "\\-42"',
    function(err, stdout, stderr) {
      assert.equal(stdout, '-42\n');
      assert.equal(stderr, '');
    });

child.exec(nodejs + ' --use-strict -p process.execArgv',
    function(status, stdout, stderr) {
      assert.equal(stdout, "[ '--use-strict', '-p', 'process.execArgv' ]\n");
    });
