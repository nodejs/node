assert = require('assert');
child = require('child_process');

nodejs = process.execPath;

if (module.parent) {
  // signal we've been loaded as a module
  console.log('Loaded as a module, exiting with status code 42.');
  process.exit(42);
}

// assert that the result of the final expression is written to stdout
child.exec(nodejs + ' --eval "1337; 42"',
    function(err, stdout, stderr) {
      assert.equal(parseInt(stdout), 42);
    });

// assert that module loading works
child.exec(nodejs + ' --eval "require(\'' + __filename + '\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });

// module path resolve bug, regression test
child.exec(nodejs + ' --eval "require(\'./test/simple/test-cli-eval.js\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });
