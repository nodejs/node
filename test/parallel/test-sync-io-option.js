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
    const execArgv = [flags.pop()];
    let args = [__filename, 'child'];
    let cntr = 0;
    args = execArgv.concat(args);
    if (!args[0]) args.shift();
    execFile(process.execPath, args, function(err, stdout, stderr) {
      assert.strictEqual(err, null);
      assert.strictEqual(stdout, '');
      if (/WARNING[\s\S]*readFileSync/.test(stderr))
        cntr++;
      if (args[0] === '--trace-sync-io') {
        assert.strictEqual(cntr, 1);
      } else if (args[0] === __filename) {
        assert.strictEqual(cntr, 0);
      } else {
        throw new Error('UNREACHABLE');
      }
      if (flags.length > 0)
        setImmediate(runTest, flags);
    });
  }(['--trace-sync-io', '']));
}
