'use strict';

module.exports = {
  blue: '',
  green: '',
  white: '',
  red: '',
  gray: '',
  clear: '',
  hasColors: false,
  refresh() {
    if (process.stderr.isTTY) {
      const hasColors = process.stderr.hasColors();
      module.exports.blue = hasColors ? '\u001b[34m' : '';
      module.exports.green = hasColors ? '\u001b[32m' : '';
      module.exports.white = hasColors ? '\u001b[39m' : '';
      module.exports.red = hasColors ? '\u001b[31m' : '';
      module.exports.gray = hasColors ? '\u001b[90m' : '';
      module.exports.clear = hasColors ? '\u001bc' : '';
      module.exports.hasColors = hasColors;
    }
  },
};

module.exports.refresh();
