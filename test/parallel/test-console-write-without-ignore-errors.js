'use strict';
const common = require('../common');
const { Console } = require('console');
const { Writable } = require('stream');
const assert = require('assert');
const ignoreErrors = false;
const expected = 'foobarbaz';

{
  const out = new Writable({
    write: common.mustCall((chunk, enc, callback) => {
      const actual = chunk.toString();
      assert.strictEqual(expected + '\n', actual);
    })
  });
  
  const c = new Console(out, out, ignoreErrors);
  c.log(expected);
}

assert.throws(() => {
  const err = new Writable({
    write: common.mustCall((chunk, enc, callback) => {
      throw new Error('foobar');
    })
  });
  const c = new Console(err, err, ignoreErrors);
  c.log(expected);
}, /^Error: foobar$/);