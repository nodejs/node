'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const test = (stream) => {
  const err = new Error('DESTROYED');
  stream.destroy(err);
  stream.on('open', common.mustNotCall());
  stream.on('ready', common.mustNotCall());
  stream.on('close', common.mustCall());
  stream.on('error', common.mustCall((er) => {
    assert.strictEqual(err, er);
  }));
}

test(fs.createReadStream(__filename));
test(fs.createWriteStream(`${tmpdir.path}/dummy`));
