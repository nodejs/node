'use strict';
const common = require('../common');

if (!process.stdin.isTTY) {
  common.skip('does not apply on non-TTY stdin');
}

process.stdin.destroy();
process.stdin.setRawMode(true);
