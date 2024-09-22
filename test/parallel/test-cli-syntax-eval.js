'use strict';

const common = require('../common');
const assert = require('assert');
const { exec } = require('child_process');

// The execPath might contain chars that should be escaped in a shell context.
// On non-Windows, we can pass the path via the env; `"` is not a valid char on
// Windows, so we can simply pass the path.
const execNode = (args, callback) => exec(
  `"${common.isWindows ? process.execPath : '$NODE'}" ${args}`,
  common.isWindows ? undefined : { env: { ...process.env, NODE: process.execPath } },
  callback,
);


// Should throw if -c and -e flags are both passed
['-c', '--check'].forEach(function(checkFlag) {
  ['-e', '--eval'].forEach(function(evalFlag) {
    const args = [checkFlag, evalFlag, 'foo'];
    execNode(args.join(' '), common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err instanceof Error, true);
      assert.strictEqual(err.code, 9);
      assert(
        stderr.startsWith(
          `${process.execPath}: either --check or --eval can be used, not both`
        )
      );
    }));
  });
});
