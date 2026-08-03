'use strict';
const common = require('../common');
const assert = require('assert');

const { Writable } = require('stream');

{
  const w = new Writable({
    write() {}
  });
  assert.strictEqual(w.writable, true);
  w.destroy();
  assert.strictEqual(w.writable, false);
}

{
  const w = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      callback(new Error());
    })
  });
  assert.strictEqual(w.writable, true);
  w.write('asd');
  assert.strictEqual(w.writable, false);
  w.on('error', common.mustCall());
}

{
  const w = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      process.nextTick(() => {
        callback(new Error());
        assert.strictEqual(w.writable, false);
      });
    })
  });
  w.write('asd');
  w.on('error', common.mustCall());
}

{
  const w = new Writable({
    write: common.mustNotCall()
  });
  assert.strictEqual(w.writable, true);
  w.end();
  assert.strictEqual(w.writable, false);
}
