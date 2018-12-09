'use strict';

const common = require('../common');
const assert = require('assert');
const { exec } = require('child_process');

const node = process.execPath;

// should throw if -c and -e flags are both passed
['-c', '--check'].forEach(function(checkFlag) {
  ['-e', '--eval'].forEach(function(evalFlag) {
    const args = [checkFlag, evalFlag, 'foo'];
    const cmd = [node, ...args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err instanceof Error, true);
      assert.strictEqual(err.code, 9);
      assert(
        stderr.startsWith(
          `${node}: either --check or --eval can be used, not both`
        )
      );
    }));
  });
});
