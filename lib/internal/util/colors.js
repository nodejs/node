'use strict';

const {
  ArrayPrototypeForEach,
  ObjectKeys,
} = primordials;

let internalTTy;
function lazyInternalTTY() {
  internalTTy ??= require('internal/tty');
  return internalTTy;
}

const colorsMap = {
  blue: '\u001b[34m',
  green: '\u001b[32m',
  white: '\u001b[39m',
  yellow: '\u001b[33m',
  red: '\u001b[31m',
  gray: '\u001b[90m',
  clear: '\u001b[0m',
};

module.exports = {
  hasColors: false,
  shouldColorize(stream) {
    if (process.env.FORCE_COLOR !== undefined) {
      return lazyInternalTTY().getColorDepth() > 2;
    }
    return stream?.isTTY && (
      typeof stream.getColorDepth === 'function' ?
        stream.getColorDepth() > 2 : true);
  },
  refresh() {
    const isTTY = process.stderr.isTTY;
    const hasColors = isTTY && module.exports.shouldColorize(process.stderr);
    ArrayPrototypeForEach(ObjectKeys(colorsMap), (key) => {
      module.exports[key] = hasColors ? colorsMap[key] : '';
    });

    module.exports.hasColors = hasColors;
  },
};

module.exports.refresh();
