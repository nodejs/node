'use strict';
const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');
{
  const w = new Writable();
  w.end();

  w.end('asd', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));

  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
}
