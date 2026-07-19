'use strict';

const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  read.on('end', common.mustNotCall('no end event'));
  read.on('close', common.mustCall());
  read.on('error', common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
  }));

  read[Symbol.asyncDispose]().then(common.mustCall(() => {
    assert.strictEqual(read.errored.name, 'AbortError');
    assert.strictEqual(read.destroyed, true);
  }));
}

(async () => {
  await using read = new Readable({
    read() {}
  });
  read.resume();

  read.on('end', common.mustNotCall('no end event'));
  read.on('close', common.mustCall());
  read.on('error', common.mustCall(function(err) {
    assert.strictEqual(err.name, 'AbortError');
    assert.strictEqual(this.errored.name, 'AbortError');
    assert.strictEqual(this.destroyed, true);
  }));
})().then(common.mustCall());
