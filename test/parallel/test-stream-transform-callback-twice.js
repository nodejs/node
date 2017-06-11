'use strict';
const common = require('../common');
const assert = require('assert');
const { Transform } = require('stream');
const stream = new Transform({
  transform(chunk, enc, cb) { cb(); cb(); }
});

stream.on('error', common.mustCall((err) => {
  assert.strictEqual(err.toString(),
                     'Error: write callback called multiple times');
}));

stream.write('foo');
