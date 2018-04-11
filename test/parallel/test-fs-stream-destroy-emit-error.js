'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

test(fs.createReadStream(__filename));
test(fs.createWriteStream(`${tmpdir.path}/dummy`));

function test(stream) {
  const err = new Error('DESTROYED');
  stream.on('open', function() {
    stream.destroy(err);
  });
  stream.on('error', common.mustCall(function(err_) {
    assert.strictEqual(err_, err);
  }));
}
