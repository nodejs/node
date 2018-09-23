'use strict';

// Do not read filesystem when creating AssertionError messages for code in
// builtin modules.

require('../common');
const assert = require('assert');

if (process.argv[2] !== 'child') {
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const { spawnSync } = require('child_process');
  const { output, status, error } =
    spawnSync(process.execPath,
              ['--expose-internals', process.argv[1], 'child'],
              { cwd: tmpdir.path, env: process.env });
  assert.ifError(error);
  assert.strictEqual(status, 0, `Exit code: ${status}\n${output}`);
} else {
  const EventEmitter = require('events');
  const { errorCache } = require('internal/assert');
  const { writeFileSync } = require('fs');
  const e = new EventEmitter();

  e.on('hello', assert);

  let threw = false;
  try {
    e.emit('hello', false);
  } catch (err) {
    const frames = err.stack.split('\n');
    const [, filename, line, column] = frames[1].match(/\((.+):(\d+):(\d+)\)/);
    // Reset the cache to check again
    const size = errorCache.size;
    errorCache.delete(`${filename}${line - 1}${column - 1}`);
    assert.strictEqual(errorCache.size, size - 1);
    const data = `${'\n'.repeat(line - 1)}${' '.repeat(column - 1)}` +
                 'ok(failed(badly));';

    writeFileSync(filename, data);
    assert.throws(
      () => e.emit('hello', false),
      {
        message: 'false == true'
      }
    );
    threw = true;

  }
  assert(threw);
}
