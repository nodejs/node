'use strict';

const common = require('../common');
const { Readable, Writable } = require('stream');
const assert = require('assert');

{
  const w = new Writable({
    write() {

    }
  });
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
  }));
  w.destroy();
}

{
  const r = new Readable({
    read() {

    }
  });
  r.on('error', common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
  }));
  r.destroy();
}
