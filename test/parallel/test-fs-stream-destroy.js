'use strict';
const common = require('../common');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

test(fs.createReadStream(__filename));
test(fs.createWriteStream(`${tmpdir.path}/dummy`));

function test(stream) {
  stream.on('open', common.mustNotCall());
  stream.on('ready', common.mustNotCall());
  stream.destroy();
}
