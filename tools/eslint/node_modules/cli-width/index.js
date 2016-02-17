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
      if (process.env.CLI_WIDTH) {
        var width = parseInt(process.env.CLI_WIDTH, 10);

        if (!isNaN(width)) {
          return width;
        }
      }

      return exports.defaultWidth;
    }
  }
};
