'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');

let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

module.exports = { nextDb };
