'use strict';

/*!
 * window-size <https://github.com/jonschlinkert/window-size>
 *
 * Copyright (c) 2014-2015 Jon Schlinkert
 * Licensed under the MIT license.
 */

var tty = require('tty');

module.exports = (function () {
  var width;
  var height;

  if (tty.isatty(1) && tty.isatty(2)) {
    if (process.stdout.getWindowSize) {
      width = process.stdout.getWindowSize(1)[0];
      height = process.stdout.getWindowSize(1)[1];
    } else if (tty.getWindowSize) {
      width = tty.getWindowSize()[1];
      height = tty.getWindowSize()[0];
    } else if (process.stdout.columns && process.stdout.rows) {
      height = process.stdout.rows;
      width = process.stdout.columns;
    }
  } else {
    Error('window-size could not get size with tty or process.stdout.');
  }

  return {height: height, width: width};
})();
