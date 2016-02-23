'use strict';

require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;

if (process.argv[2] === 'child') {
  setImmediate(function() {
    require('fs').readFileSync(__filename);
    process.exit();
  });

} else {
  (function runTest(flags) {
    var execArgv = [flags.pop()];
    var args = [__filename, 'child'];
    var cntr = 0;
    args = execArgv.concat(args);
    if (!args[0]) args.shift();
    execFile(process.execPath, args, function(err, stdout, stderr) {
      assert.equal(err, null);
      assert.equal(stdout, '');
      if (/WARNING[\s\S]*fs\.readFileSync/.test(stderr))
        cntr++;
      if (args[0] === '--trace-sync-io') {
        assert.equal(cntr, 1);
      } else if (args[0] === __filename) {
        assert.equal(cntr, 0);
      } else {
        throw new Error('UNREACHABLE');
      }
      if (flags.length > 0)
        setImmediate(runTest, flags);
    });
  }(['--trace-sync-io', '']));
}
