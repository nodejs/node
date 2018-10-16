'use strict';

const common = require('../common');
const assert = require('assert');
const { execFile, execFileSync } = require('child_process');
const { getSystemErrorName } = require('util');
const fixtures = require('../common/fixtures');

const fixture = fixtures.path('exit.js');
const execOpts = { encoding: 'utf8', shell: true };

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

{
  // Verify the shell option works properly
  execFile(process.execPath, [fixture, 0], execOpts, common.mustCall((err) => {
    assert.ifError(err);
  }));
}

{
  // Verify the shell option is only passed explicitly
  // in the options object and not inherited from the
  // object prototype
  Object.prototype.shell = true;

  execFile(
    'date',
    ['; echo baz'],
    common.mustCall((e, stdout) => {
      assert.notStrictEqual(e, null);
      assert.strictEqual(stdout.trim().indexOf('baz'), -1);
    })
  );

  Reflect.deleteProperty(Object.prototype, 'shell');
}

{
  // Verify the shell option is only passed explicitly
  // in the options object and not inherited from the
  // object prototype
  Object.prototype.shell = true;

  let assertionExpected = 0;
  try {
    execFileSync(
      'date',
      ['; echo baz']
    );
  } catch (err) {
    assertionExpected++;
    assert.strictEqual(err.status, 1);
  }

  assert.strictEqual(assertionExpected, 1);
  Reflect.deleteProperty(Object.prototype, 'shell');
}
