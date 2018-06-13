'use strict';
const common = require('../common');
const assert = require('assert');
const { createReadStream, createWriteStream } = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

test(createReadStream(__filename));
test(createWriteStream(`${tmpdir.path}/dummy`));

function test(stream) {
  const err = new Error('DESTROYED');
  stream.on('open', function() {
    stream.destroy(err);
  });
  stream.on('error', common.mustCall(function(err_) {
    assert.strictEqual(err_, err);
  }));
}
