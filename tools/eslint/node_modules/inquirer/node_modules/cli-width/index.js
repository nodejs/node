'use strict';

exports = module.exports = cliWidth;
exports.defaultWidth = 0;

function cliWidth() {
  if (process.stdout.getWindowSize) {
    return process.stdout.getWindowSize()[0];
  }
  else {
    var tty = require('tty');

    if (tty.getWindowSize) {
      return tty.getWindowSize()[1];
    }
    else {
      return exports.defaultWidth;
    }
  }
};
