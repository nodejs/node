'use strict';

var tryit = require('tryit');

module.exports = function isResolvable(moduleId) {
  if (typeof moduleId !== 'string') {
    throw new TypeError(
      moduleId +
      ' is not a string. A module identifier to be checked if resolvable is required.'
    );
  }

  var result;
  tryit(function() {
    require.resolve(moduleId);
  }, function(err) {
    if (err) {
      result = false;
      return;
    }

    result = true;
  });

  return result;
};
