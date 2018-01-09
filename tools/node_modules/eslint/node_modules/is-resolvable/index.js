'use strict';

var inspect = require('util').inspect;

module.exports = function isResolvable(moduleId) {
  if (typeof moduleId !== 'string') {
    throw new TypeError(inspect(moduleId) + ' is not a string. Expected a valid Node.js module identifier (<string>), for example \'eslint\', \'./index.js\', \'./lib\'.');
  }

  try {
    require.resolve(moduleId);
    return true;
  } catch (err) {
    return false;
  }
};
