
var binding = process.binding('stdio');

if (process.platform === 'win32') {
  exports = module.exports = require('tty_win32');
} else {
  exports = module.exports = require('tty_posix');
}

exports.isatty = binding.isatty;
exports.setRawMode = binding.setRawMode;
exports.getWindowSize = binding.getWindowSize;
exports.setWindowSize = binding.setWindowSize;
