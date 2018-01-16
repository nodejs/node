'use strict';

const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const { getSystemErrorName } = require('util');
const fixtures = require('../common/fixtures');

const fixture = fixtures.path('exit.js');

{
  execFile(
    process.execPath,
    [fixture, 42],
    common.mustCall((e) => {
      // Check that arguments are included in message
      assert.strictEqual(e.message.trim(),
                         `Command failed: ${process.execPath} ${fixture} 42`);
      assert.strictEqual(e.code, 42);
    })
  );
}

{
  // Verify that negative exit codes can be translated to UV error names.
  const errorString = `Error: Command failed: ${process.execPath}`;
  const code = -1;
  const callback = common.mustCall((err, stdout, stderr) => {
    assert.strictEqual(err.toString().trim(), errorString);
    assert.strictEqual(err.code, getSystemErrorName(code));
    assert.strictEqual(err.killed, true);
    assert.strictEqual(err.signal, null);
    assert.strictEqual(err.cmd, process.execPath);
    assert.strictEqual(stdout.trim(), '');
    assert.strictEqual(stderr.trim(), '');
  });
  const child = execFile(process.execPath, callback);

  child.kill();
  child.emit('close', code, null);
}
