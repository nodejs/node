'use strict';
const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const path = require('path');
const uv = process.binding('uv');

const fixture = path.join(common.fixturesDir, 'exit.js');

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
    assert.strictEqual(err.code, uv.errname(code));
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
