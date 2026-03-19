'use strict';

const { promisify } = require('internal/util');
const { exec, execFile } = require('child_process');

module.exports = {
  exec: promisify(exec),
  execFile: promisify(execFile),
};
