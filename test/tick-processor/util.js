'use strict';

// Utilities for the tick-processor tests
const {
  isWindows,
  isSunOS,
  isAIX,
  isLinuxPPCBE,
  isFreeBSD,
} = require('../common');

module.exports = {
  isCPPSymbolsNotMapped: isWindows ||
                         isSunOS ||
                         isAIX ||
                         isLinuxPPCBE ||
                         isFreeBSD,
};
