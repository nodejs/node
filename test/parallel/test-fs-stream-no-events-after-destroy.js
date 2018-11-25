'use strict';
const common = require('../common');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

test(fs.createReadStream(__filename));
test(fs.createWriteStream(`${tmpdir.path}/dummy`));

function test(stream) {
  const err = new Error('DESTROYED');
  stream.destroy(err);
  stream.on('open', common.mustNotCall());
  stream.on('ready', common.mustNotCall());
  stream.on('close', common.mustCall());
}
