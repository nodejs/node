// Flags: --expose-internals

'use strict';

require('../common');

const assert = require('assert');
const EventEmitter = require('events');
const { errorCache } = require('internal/assert');
const { writeFileSync, unlinkSync } = require('fs');

// Do not read filesystem for error messages in builtin modules.
{
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
    try {
      writeFileSync(filename, data);
      assert.throws(
        () => e.emit('hello', false),
        {
          message: 'false == true'
        }
      );
      threw = true;
    } finally {
      unlinkSync(filename);
    }
  }
  assert(threw);
}
