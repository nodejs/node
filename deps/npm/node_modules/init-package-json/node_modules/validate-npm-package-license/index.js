var spdx = require('spdx');
var correct = require('spdx-correct');

module.exports = function(argument) {
  if (spdx.valid(argument)) {
    return {
      validForNewPackages: true,
      validForOldPackages: true
    };
  } else {
    var warnings = [
      'license should be a valid SPDX license expression'
    ];
    var corrected = correct(argument);
    if (corrected) {
      warnings.push(
        'license is similar to the valid expression "' + corrected + '"'
      );
    }
    return {
      validForOldPackages: false,
      validForNewPackages: false,
      warnings: warnings
    };
  }
};
