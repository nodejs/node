'use strict';

var rc = require('rc'),
    untildify = require('untildify'),
    substitute = require('shellsubstitute');

var path = require('path');


module.exports = function () {
  var rcPrefix = rc('npm', null, []).prefix;

  if (rcPrefix) {
    return untildify(substitute(rcPrefix, process.env));
  }
  else if (process.platform == 'win32') {
    return path.dirname(process.execPath);
  }
  else {
    return path.resolve(process.execPath, '../..');
  }
};
