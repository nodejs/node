'use strict';
const common = require('./index.js');

exports.skipIfNoWatchModeSignals = function() {
  if (common.isWindows) {
    common.skip('no signals on Windows');
  }

  if (common.isIBMi) {
    common.skip('IBMi does not support `fs.watch()`');
  }

  if (common.isAIX) {
    common.skip('folder watch capability is limited in AIX.');
  }
};
