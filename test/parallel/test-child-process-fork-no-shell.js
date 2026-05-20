'use strict';
// This test verifies that the shell option is not supported by fork().
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const expected = common.isWindows ? '%foo%' : '$foo';

if (process.argv[2] === undefined) {
  const child = cp.fork(__filename, [expected], {
    shell: true,
    env: { ...process.env, foo: 'bar' }
  });

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
} else {
  assert.strictEqual(process.argv[2], expected);
}
