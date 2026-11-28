'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');

let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return tmpdir.resolve(`database-${cnt++}.db`);
}

module.exports = { nextDb };
