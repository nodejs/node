'use strict';
const assert = require('assert');

const { isReadable, isWritable, Writable, Readable } = require('stream');

{

  assert.strictEqual(isReadable(new Readable()), true);
  assert.strictEqual(isReadable(new Writable()), false);
  assert.strictEqual(isWritable(new Writable()), true);
  assert.strictEqual(isWritable(new Readable()), false);
}
