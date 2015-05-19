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
    var stderr = '';

    child.stdout.on('data', function(chunk) {
      throw new Error('UNREACHABLE');
    });

    child.stderr.on('data', function(chunk) {
      stderr += chunk.toString();
    });

    child.on('close', function() {
      var cntr1 = (stderr.match(/WARNING/g) || []).length;
      var cntr2 = (stderr.match(/fs\.readFileSync/g) || []).length;
      assert.equal(cntr1, cntr2);
      if (execArgv[0] === '--trace-sync-io') {
        // Prints 4 WARNINGS for --trace-sync-io. 1 for each sync call
        // inside readFileSync
        assert.equal(cntr1, 4);
      } else if (execArgv[0] === ' ')
        assert.equal(cntr1, 0);
      else
        throw new Error('UNREACHABLE');

      if (flags.length > 0)
        setImmediate(runTest, flags);
    });
  }(['--trace-sync-io', ' ']));
}

