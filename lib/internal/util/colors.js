'use strict';

let internalTTy;
function lazyInternalTTY() {
  internalTTy ??= require('internal/tty');
  return internalTTy;
}

// Color depths in bits, matching the values returned by `getColorDepth()`.
const COLORS_2 = 1;
const COLORS_16 = 4;
const COLORS_256 = 8;
const COLORS_16m = 24;

module.exports = {
  blue: '',
  green: '',
  white: '',
  red: '',
  gray: '',
  clear: '',
  reset: '',
  hasColors: false,
  COLORS_2,
  COLORS_16,
  COLORS_256,
  COLORS_16m,
  // Number of bits of color the stream supports, as reported by
  // `tty.WriteStream.prototype.getColorDepth()`. `FORCE_COLOR` takes precedence
  // over the stream, since it describes the terminal the output ends up in.
  getColorDepth(stream) {
    if (process.env.FORCE_COLOR !== undefined) {
      return lazyInternalTTY().getColorDepth();
    }

    if (!stream?.isTTY) {
      return COLORS_2;
    }

    return typeof stream.getColorDepth === 'function' ?
      stream.getColorDepth() :
      COLORS_16;
  },
  // Depth to assume when the stream is not validated. `FORCE_COLOR` is then the
  // only hint about the terminal capabilities; without it, assume that whoever
  // opted out of the stream validation can handle the full color range.
  getForcedColorDepth() {
    if (process.env.FORCE_COLOR === undefined) {
      return COLORS_16m;
    }

    return lazyInternalTTY().getColorDepth();
  },
  shouldColorize(stream) {
    return module.exports.getColorDepth(stream) > 2;
  },
  refresh() {
    if (module.exports.shouldColorize(process.stderr)) {
      module.exports.blue = '\u001b[34m';
      module.exports.green = '\u001b[32m';
      module.exports.white = '\u001b[39m';
      module.exports.yellow = '\u001b[33m';
      module.exports.red = '\u001b[31m';
      module.exports.gray = '\u001b[90m';
      module.exports.clear = '\u001bc';
      module.exports.reset = '\u001b[0m';
      module.exports.hasColors = true;
    } else {
      module.exports.blue = '';
      module.exports.green = '';
      module.exports.white = '';
      module.exports.yellow = '';
      module.exports.red = '';
      module.exports.gray = '';
      module.exports.clear = '';
      module.exports.reset = '';
      module.exports.hasColors = false;
    }
  },
};

module.exports.refresh();
