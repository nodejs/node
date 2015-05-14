'use strict';

const assert = require('assert');
const spawn = require('child_process').spawn;


if (process.argv[2] === 'child') {
  setImmediate(function() {
    require('fs').readFileSync(__filename);
    process.exit();
  });

} else {
  (function runTest(flags) {
    var execArgv = [flags.pop()];
    var args = [__filename, 'child'];
    var child = spawn(process.execPath, execArgv.concat(args));
    var cntr = 0;

    child.stdout.on('data', function(chunk) {
      throw new Error('UNREACHABLE');
    });

    child.stderr.on('data', function(chunk) {
      // Prints twice for --trace-sync-io. First for the require() and second
      // for the fs operation.
      if (/^WARNING[\s\S]*fs\.readFileSync/.test(chunk.toString()))
        cntr++;
    });

    child.on('exit', function() {
      if (execArgv[0] === '--trace-sync-io')
        assert.equal(cntr, 2);
      else if (execArgv[0] === ' ')
        assert.equal(cntr, 0);
      else
        throw new Error('UNREACHABLE');

      if (flags.length > 0)
        setImmediate(runTest, flags);
    });
  }(['--trace-sync-io', ' ']));
}

