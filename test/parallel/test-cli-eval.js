'use strict';
if (module.parent) {
  // signal we've been loaded as a module
  console.log('Loaded as a module, exiting with status code 42.');
  process.exit(42);
}

const common = require('../common');
const assert = require('assert');
const child = require('child_process');
const path = require('path');
const nodejs = '"' + process.execPath + '"';


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

  child.exec(cmd + "'[]'", common.mustCall(
      function(err, stdout, stderr) {
        assert.equal(stdout, '[]\n');
        assert.equal(stderr, '');
      }));
});

// assert that module loading works
child.exec(nodejs + ' --eval "require(\'' + filename + '\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });

// Check that builtin modules are pre-defined.
child.exec(nodejs + ' --print "os.platform()"',
    function(status, stdout, stderr) {
      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout.trim(), require('os').platform());
    });

// module path resolve bug, regression test
child.exec(nodejs + ' --eval "require(\'./test/parallel/test-cli-eval.js\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });

// Missing argument should not crash
child.exec(nodejs + ' -e', common.mustCall(function(status, stdout, stderr) {
  assert.notStrictEqual(status, null);
  assert.strictEqual(status.code, 9);
}));

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

// Regression test for https://github.com/nodejs/node/issues/3574
const emptyFile = path.join(common.fixturesDir, 'empty.js');
child.exec(nodejs + ` -e 'require("child_process").fork("${emptyFile}")'`,
    function(status, stdout, stderr) {
      assert.equal(stdout, '');
      assert.equal(stderr, '');
    });
